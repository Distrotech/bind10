#!/usr/bin/env python3.2

# Copyright (C) 2012  Internet Systems Consortium.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SYSTEMS CONSORTIUM
# DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
# INTERNET SYSTEMS CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

from isc.dns import *
import dns_cache
from dns_cache import SimpleDNSCache, install_root_hint
import datetime
import errno
import heapq
from optparse import OptionParser
import random
import re
import signal
import sys
import socket
import select
import time

DNS_PORT = 53

random.random()

LOGLVL_INFO = 0
LOGLVL_DEBUG1 = 1
LOGLVL_DEBUG3 = 3    # rare event, but can happen in real world
LOGLVL_DEBUG5 = 5           # some unexpected event, but sometimes happen
LOGLVL_DEBUG10 = 10         # detailed trace

def get_soa_ttl(soa_rdata):
    '''Extract the serial field of SOA RDATA and return it as a Serial object.

    Borrowed from xfrin.
    '''
    return int(soa_rdata.to_text().split()[6])

class MiniResolverException(Exception):
    pass

class InternalLame(Exception):
    pass

class ResQuery:
    '''Encapsulates a single query from the resolver.'''
    # Constant of timeout for eqch query in seconds
    QUERY_TIMEOUT = 3

    def __init__(self, res_ctx, qid, ns_addr):
        self.res_ctx = res_ctx
        self.qid = qid
        self.ns_addr = ns_addr
        self.expire = time.time() + self.QUERY_TIMEOUT
        self.timer = None       # will be set when timer is associated

class ResolverStatistics:
    def __init__(self):
        self.query_timeout = 0
        self.response_broken = 0
        self.response_truncated = 0
        self.__response_stat = {} # SimpleDNSCache.RESP_xxx => counter
        for type in SimpleDNSCache.RESP_DESCRIPTION.keys():
            self.__response_stat[type] = 0

    def update_response_stat(self, type):
        self.__response_stat[type] += 1

    def dump(self, f):
        f.write('Response statistics:\n')
        f.write('  query timeout: %d\n' % self.query_timeout)
        f.write('  broken responses: %d\n' % self.response_broken)
        f.write('  truncated responses: %d\n' % self.response_truncated)

        descriptions = list(SimpleDNSCache.RESP_DESCRIPTION.keys())
        descriptions.sort()
        for type in descriptions:
            f.write('  %s: %d\n' % (SimpleDNSCache.RESP_DESCRIPTION[type],
                                    self.__response_stat[type]))

class ResolverContext:
    CNAME_LOOP_MAX = 15
    FETCH_DEPTH_MAX = 8
    DEFAULT_NEGATIVE_TTL = 10800 # used when SOA is missing, value from BIND 9.
    SERVFAIL_TTL = 1800 # cache TTL for 'SERVFAIL' results.  BIND9's lame-ttl.

    def __init__(self, sock4, sock6, renderer, qname, qclass, qtype, cache,
                 query_table, stat, nest=0):
        self.__sock4 = sock4
        self.__sock6 = sock6
        self.__msg = Message(Message.RENDER)
        self.__renderer = renderer
        self.__qname = qname
        self.__qclass = qclass
        self.__qtype = qtype
        self.__cache = cache
        self.__create_query()
        self.__nest = nest      # CNAME loop prevention
        self.__debug_level = LOGLVL_INFO
        self.__fetch_queries = set()
        self.__aux_fetch_queries = set()
        self.__parent = None    # set for internal fetch contexts
        self.__cur_zone = None
        self.__cur_ns_addr = None
        self.__qtable = query_table
        self.__stat = stat

    def set_debug_level(self, level):
        self.__debug_level = level
        self.dprint(LOGLVL_DEBUG10, 'created')

    def dprint(self, level, msg, params=[]):
        '''Dump a debug/log message.'''
        if self.__debug_level < level:
            return
        date_time = str(datetime.datetime.today())
        postfix = '[%s/%s/%s at %s' % (self.__qname.to_text(True),
                                       str(self.__qclass), str(self.__qtype),
                                       self.__cur_zone)
        if self.__cur_ns_addr is not None:
            postfix += ' to ' + self.__cur_ns_addr[0]
        postfix += ']'
        if level > LOGLVL_DEBUG1:
            postfix += ' ' + str(self)
        sys.stdout.write(('%s ' + msg + ' %s\n') %
                         tuple([date_time] + [str(p) for p in params] +
                               [postfix]))

    def get_aux_queries(self):
        return list(self.__fetch_queries) + list(self.__aux_fetch_queries)

    def __create_query(self):
        '''Create a template query.  QID will be filled on send.'''
        self.__msg.clear(Message.RENDER)
        self.__msg.set_opcode(Opcode.QUERY())
        self.__msg.set_rcode(Rcode.NOERROR())
        self.__msg.add_question(Question(self.__qname, self.__qclass,
                                         self.__qtype))

    def start(self):
        # identify the deepest zone cut.  for the last resort, we also fall
        # back to the full recursion from root, assuming we can find an
        # available server there.
        for qname in [self.__qname, Name('.')]:
            self.__cur_zone, nameservers = \
                self.__find_deepest_nameserver(qname)
            self.dprint(LOGLVL_DEBUG10, 'Located deepest zone cut')

            # get server addresses.  Don't bother to fetch missing addresses;
            # we cannot fail in this context, so we'd rather fall back to root.
            (self.__ns_addr4, self.__ns_addr6) = \
                self.__find_ns_addrs(nameservers, False)
            cur_ns_addr = self.__try_next_server()
            if cur_ns_addr is not None:
                return self.__qid, cur_ns_addr
        raise MiniResolverException('unexpected: no available server found')

    def query_timeout(self, ns_addr):
        self.dprint(LOGLVL_DEBUG5, 'query timeout')
        cur_ns_addr = self.__try_next_server()
        if cur_ns_addr is not None:
            return ResQuery(self, self.__qid, cur_ns_addr)
        self.dprint(LOGLVL_DEBUG1, 'no reachable server')
        fail_rrset = RRset(self.__qname, self.__qclass, self.__qtype,
                           RRTTL(self.SERVFAIL_TTL))
        self.__cache.add(fail_rrset, SimpleDNSCache.TRUST_ANSWER,
                         (0, SimpleDNSCache.RESP_FINAL_ANSWER_NONE),
                         Rcode.SERVFAIL())
        return self.__resume_parents()

    def handle_response(self, resp_msg, resp_data):
        next_qry = None
        msglen = len(resp_data)
        try:
            if not resp_msg.get_header_flag(Message.HEADERFLAG_QR):
                self.dprint(LOGLVL_INFO,
                            'received query when expecting a response')
                self.__stat.response_broken += 1
                raise InternalLame('lame server')
            if resp_msg.get_rr_count(Message.SECTION_QUESTION) != 1:
                self.dprint(LOGLVL_INFO,
                            'unexpected # of question in response: %s',
                            [resp_msg.get_rr_count(Message.SECTION_QUESTION)])
                self.__stat.response_broken += 1
                raise InternalLame('lame server')
            question = resp_msg.get_question()[0]
            if (question.get_name() != self.__qname or
                question.get_class() != self.__qclass or
                question.get_type() != self.__qtype):
                self.dprint(LOGLVL_INFO, 'unexpected response: ' +
                            'query mismatch actual=%s/%s/%s',
                            [question.get_name(), question.get_class(),
                             question.get_type()])
                self.__stat.response_broken += 1
                raise InternalLame('lame server')
            if resp_msg.get_qid() != self.__qid:
                self.dprint(LOGLVL_INFO, 'unexpected response: '
                            'QID mismatch; expected=%s, actual=%s',
                            [self.__qid, resp_msg.get_qid()])
                self.__stat.response_broken += 1
                raise InternalLame('lame server')

            resp_type = self.__get_response_type(resp_msg, resp_data)
            self.__stat.update_response_stat(resp_type)
            if resp_msg.get_header_flag(Message.HEADERFLAG_TC):
                self.__stat.response_truncated += 1
            respinfo = (msglen, resp_type)

            # Look into the response
            if (resp_msg.get_header_flag(Message.HEADERFLAG_AA) or
                self.__is_cname_response(resp_msg)):
                next_qry = self.__handle_auth_answer(resp_msg, respinfo)
                self.__handle_auth_othersections(resp_msg)
            elif (resp_msg.get_rr_count(Message.SECTION_ANSWER) == 0 and
                  resp_msg.get_rr_count(Message.SECTION_AUTHORITY) == 0 and
                  (resp_msg.get_rcode() == Rcode.NOERROR() or
                   resp_msg.get_rcode() == Rcode.NXDOMAIN())):
                # Some servers return a negative response without setting AA.
                # (Leave next_qry None)
                self.__handle_negative_answer(resp_msg, respinfo)
            elif (resp_msg.get_rcode() == Rcode.NOERROR() and
                  not resp_msg.get_header_flag(Message.HEADERFLAG_AA)):
                authorities = resp_msg.get_section(Message.SECTION_AUTHORITY)
                ns_name = None
                for ns_rrset in authorities:
                    if ns_rrset.get_type() == RRType.NS():
                        ns_name =  ns_rrset.get_name()
                        cmp_reln = \
                            ns_name.compare(self.__cur_zone).get_relation()
                        if cmp_reln != NameComparisonResult.SUBDOMAIN:
                            raise InternalLame('lame server: ' +
                                               'delegation not for subdomain')
                        ns_addr = self.__handle_referral(resp_msg, ns_rrset,
                                                         respinfo)
                        if ns_addr is not None:
                            next_qry = ResQuery(self, self.__qid, ns_addr)
                        elif len(self.__fetch_queries) == 0:
                            raise InternalLame('no further recursion possible')
                        break
                if ns_name is None:
                    raise InternalLame('delegation with no NS')
            else:
                raise InternalLame('lame server, rcode=' +
                                   str(resp_msg.get_rcode()))
        except InternalLame as ex:
            self.dprint(LOGLVL_INFO, '%s', [ex])
            ns_addr = self.__try_next_server()
            if ns_addr is not None:
                next_qry = ResQuery(self, self.__qid, ns_addr)
            else:
                self.dprint(LOGLVL_DEBUG1, 'no usable server')
                fail_rrset = RRset(self.__qname, self.__qclass, self.__qtype,
                                   RRTTL(self.SERVFAIL_TTL))
                self.__cache.add(fail_rrset, SimpleDNSCache.TRUST_ANSWER,
                                 (0, SimpleDNSCache.RESP_FINAL_ANSWER_NONE),
                                 Rcode.SERVFAIL())
        if next_qry is None:
             next_qry = self.__resume_parents()
        return next_qry

    def __get_response_type(self, resp_msg, resp_data):
        if (resp_msg.get_header_flag(Message.HEADERFLAG_AA) and
            resp_msg.get_rcode() == Rcode.NOERROR() and
            resp_msg.get_rr_count(Message.SECTION_ANSWER) > 0):
            answer = resp_msg.get_section(Message.SECTION_ANSWER)[0]
            # typical positive answer
            is_cname = (self.__qtype != RRType.CNAME() and
                        self.__qtype != RRType.ANY() and
                        answer.get_type() == RRType.CNAME())
            is_final = (answer.get_name() == self.__qname)
            offset_to_answer = 12 + self.__qname.get_length() + 4
            is_final_compressed = (is_final and
                                   resp_data[offset_to_answer] == 0xc0 and
                                   resp_data[offset_to_answer + 1] == 12)
            if is_final_compressed and not is_cname:
                return SimpleDNSCache.RESP_FINAL_ANSWER_COMPRESSED
            elif is_final_compressed and is_cname:
                return SimpleDNSCache.RESP_CNAME_ANSWER_COMPRESSED
            elif is_final and not is_cname:
                return SimpleDNSCache.RESP_FINAL_ANSWER_UNCOMPRESSED
            elif is_final and is_cname:
                return SimpleDNSCache.RESP_CNAME_ANSWER_UNCOMPRESSED
            else:
                return SimpleDNSCache.RESP_ANSWER_UNEXPECTED
        elif (resp_msg.get_header_flag(Message.HEADERFLAG_AA) and
              resp_msg.get_rcode() == Rcode.NXDOMAIN() and
              resp_msg.get_rr_count(Message.SECTION_ANSWER) == 0):
            # typical NXDOMAIN answer
            if resp_msg.get_rr_count(Message.SECTION_AUTHORITY) > 0:
                auth = resp_msg.get_section(Message.SECTION_AUTHORITY)[0]
                if auth.get_type() == RRType.SOA():
                    return SimpleDNSCache.RESP_NXDOMAIN_SOA
            elif resp_msg.get_rr_count(Message.SECTION_AUTHORITY) == 0:
                return SimpleDNSCache.RESP_NXDOMAIN_NOAUTH
            return SimpleDNSCache.RESP_NXDOMAIN_UNEXPECTED
        elif (resp_msg.get_header_flag(Message.HEADERFLAG_AA) and
              resp_msg.get_rcode() == Rcode.NOERROR() and
              resp_msg.get_rr_count(Message.SECTION_ANSWER) == 0):
            # typical NXRRSET answer
            if resp_msg.get_rr_count(Message.SECTION_AUTHORITY) > 0:
                auth = resp_msg.get_section(Message.SECTION_AUTHORITY)[0]
                if auth.get_type() == RRType.SOA():
                    return SimpleDNSCache.RESP_NXRRSET_SOA
            elif resp_msg.get_rr_count(Message.SECTION_AUTHORITY) == 0:
                return SimpleDNSCache.RESP_NXRRSET_NOAUTH
            return SimpleDNSCache.RESP_NXRRSET_UNEXPECTED
        elif (not resp_msg.get_header_flag(Message.HEADERFLAG_AA) and
              resp_msg.get_rcode() == Rcode.NOERROR() and
              resp_msg.get_rr_count(Message.SECTION_ANSWER) == 0 and
              resp_msg.get_rr_count(Message.SECTION_AUTHORITY) > 0):
            # typical delegation
            auth = resp_msg.get_section(Message.SECTION_AUTHORITY)[0]
            if auth.get_type() == RRType.NS():
                ns_name =  auth.get_name()
                cmp_reln = ns_name.compare(self.__cur_zone).get_relation()
                if cmp_reln == NameComparisonResult.SUBDOMAIN:
                    # valid delegation
                    if self.__check_bailiwick_glue(resp_msg, auth):
                        return SimpleDNSCache.RESP_REFERRAL_WITHGLUE
                    else:
                        return SimpleDNSCache.RESP_REFERRAL_WITHOUTGLUE
            return SimpleDNSCache.RESP_REFERRAL_UNEXPECTED
        else:
            # last resort: unexpected or uncommon
            return SimpleDNSCache.RESP_UNEXPECTED

    def __check_bailiwick_glue(self, resp_msg, ns_rrset):
        '''Check if the message contains "in-bailiwick" glue in additional.'''
        ns_names = [Name(ns.to_text()) for ns in ns_rrset.get_rdata()]
        for ad_rrset in resp_msg.get_section(Message.SECTION_ADDITIONAL):
            if (ad_rrset.get_type() != RRType.A() and
                ad_rrset.get_type() != RRType.AAAA()):
                continue
            ad_name = ad_rrset.get_name()
            for ns_name in ns_names:
                if ns_name == ad_name:
                    cmp_reln = self.__cur_zone.compare(ad_name).get_relation()
                    if (cmp_reln == NameComparisonResult.EQUAL or
                        cmp_reln == NameComparisonResult.SUPERDOMAIN):
                        return True
        return False

    def __is_cname_response(self, resp_msg):
        # From BIND 9: A BIND8 server could return a non-authoritative
        # answer when a CNAME is followed.  We should treat it as a valid
        # answer.
        # This is real; for example some amazon.com servers behave that way.
        # For simplicity we just check the first answer RR.
        if (resp_msg.get_rcode() == Rcode.NOERROR() and
            resp_msg.get_rr_count(Message.SECTION_ANSWER) > 0 and
            resp_msg.get_section(Message.SECTION_ANSWER)[0].get_type() ==
            RRType.CNAME()):
            return True
        return False

    def __handle_auth_answer(self, resp_msg, respinfo):
        '''Subroutine of handle_response, handling an authoritative answer.'''
        if (resp_msg.get_rcode() == Rcode.NOERROR() or
            resp_msg.get_rcode() == Rcode.NXDOMAIN()) and \
            resp_msg.get_rr_count(Message.SECTION_ANSWER) > 0:
            any_query = resp_msg.get_question()[0].get_type() == RRType.ANY()
            found = False
            for answer_rrset in resp_msg.get_section(Message.SECTION_ANSWER):
                if (answer_rrset.get_name() == self.__qname and
                    answer_rrset.get_class() == self.__qclass):
                    if any_query or answer_rrset.get_type() == self.__qtype:
                        found = True
                        if answer_rrset.get_type() == RRType.NS():
                            self.__check_ns_consistency(answer_rrset)
                        self.__cache.add(answer_rrset,
                                         SimpleDNSCache.TRUST_ANSWER,
                                         respinfo)
                        self.dprint(LOGLVL_DEBUG10, 'got a response: %s',
                                    [answer_rrset])
                        if not any_query:
                            # For type any query, examine all RRs; otherwise
                            # simply ignore the rest.
                            return None
                    elif answer_rrset.get_type() == RRType.CNAME():
                        return self.__handle_cname(answer_rrset, respinfo)
            if found:
                return None
            raise InternalLame('no answer found in answer section')
        elif resp_msg.get_rcode() == Rcode.NXDOMAIN() or \
                (resp_msg.get_rcode() == Rcode.NOERROR() and
                 resp_msg.get_rr_count(Message.SECTION_ANSWER) == 0):
            self.__handle_negative_answer(resp_msg, respinfo)
            return None

        raise InternalLame('unexpected answer rcode=' +
                           str(resp_msg.get_rcode()))

    def __handle_cname(self, cname_rrset, respinfo):
        self.dprint(LOGLVL_DEBUG10, 'got an alias: %s', [cname_rrset])
        # Chase CNAME with a separate resolver context with
        # loop prevention
        if self.__nest > self.CNAME_LOOP_MAX:
            self.dprint(LOGLVL_INFO, 'possible CNAME loop')
            return None

        cname = Name(cname_rrset.get_rdata()[0].to_text())

        # Examine the current cache: Sometimes it's possisble CNAME has
        # changed at the server side within a short period.
        # It's not necessarily bogus, but that confuses our experiments.
        # We consistently use cached one.
        _, cached_rrset, _ = self.__cache.find(self.__qname, self.__qclass,
                                               RRType.CNAME())
        if cached_rrset is not None:
            cached_cname = Name(cached_rrset.get_rdata()[0].to_text())
            if cname != cached_cname:
                self.dprint(LOGLVL_INFO, 'received CNAME %s' +
                            ' different from cached %s', [cname, cached_cname])
                cname = cached_cname
        else:
            self.__cache.add(cname_rrset, SimpleDNSCache.TRUST_ANSWER,
                             respinfo)

        if self.__parent is not None:
            # Don't chase CNAME in an internal fetch context.  Note that we
            # should still cache the CNAME; otherwise replay would be confused.
            self.dprint(LOGLVL_INFO, 'CNAME in internal fetch')
            return None

        cname_ctx = ResolverContext(self.__sock4, self.__sock6,
                                    self.__renderer, cname, self.__qclass,
                                    self.__qtype, self.__cache, self.__qtable,
                                    self.__stat, self.__nest + 1)
        cname_ctx.set_debug_level(self.__debug_level)
        (qid, ns_addr) = cname_ctx.start()
        if ns_addr is not None:
            return ResQuery(cname_ctx, qid, ns_addr)
        return None

    def __handle_auth_othersections(self, resp_msg):
        ns_names = []
        for auth_rrset in resp_msg.get_section(Message.SECTION_AUTHORITY):
            if auth_rrset.get_type() == RRType.NS():
                ns_owner =  auth_rrset.get_name()
                cmp_reln = ns_owner.compare(self.__cur_zone).get_relation()
                if (cmp_reln == NameComparisonResult.SUBDOMAIN or
                    cmp_reln == NameComparisonResult.EQUAL):
                    if auth_rrset.get_type() == RRType.NS():
                        self.__check_ns_consistency(auth_rrset)
                    self.__cache.add(auth_rrset,
                                     SimpleDNSCache.TRUST_AUTHAUTHORITY,
                                     (0, SimpleDNSCache.RESP_FINAL_ANSWER_NONE))
                    for ns_rdata in auth_rrset.get_rdata():
                        ns_name = Name(ns_rdata.to_text())
                        cmp_reln = \
                            ns_name.compare(self.__cur_zone).get_relation()
                        if (cmp_reln == NameComparisonResult.SUBDOMAIN or
                            cmp_reln == NameComparisonResult.EQUAL):
                            ns_names.append(Name(ns_rdata.to_text()))
        for ad_rrset in resp_msg.get_section(Message.SECTION_ADDITIONAL):
            if ad_rrset.get_type() == RRType.A() or \
                    ad_rrset.get_type() == RRType.AAAA():
                for ns_name in ns_names:
                    if ad_rrset.get_name() == ns_name:
                        self.__cache.add(ad_rrset,
                                         SimpleDNSCache.TRUST_AUTHADDITIONAL,
                                         (0, SimpleDNSCache.RESP_FINAL_ANSWER_NONE))
                        break

    def __check_ns_consistency(self, ns_rrset):
        # This check could be unnecessarily performed multiple times,
        # especially if the NS name consists a CNAME loop.  This check will
        # help prevent redundant fetch.
        if (self.__cache.find(ns_rrset.get_name(), self.__qclass,
                              RRType.NS())[1] is not None or
            self.__cache.find(ns_rrset.get_name(), self.__qclass, RRType.NS(),
                              SimpleDNSCache.FIND_ALLOW_NOANSWER,
                              SimpleDNSCache.TRUST_AUTHAUTHORITY)[1] is not None):
            return

        _, glue_ns_rrset, _ = \
            self.__cache.find(ns_rrset.get_name(), self.__qclass, RRType.NS(),
                              SimpleDNSCache.FIND_ALLOW_NOANSWER,
                              SimpleDNSCache.TRUST_GLUE)
        if glue_ns_rrset is not None:
            glue_ns_names = \
                [Name(ns.to_text()) for ns in glue_ns_rrset.get_rdata()]
        else:
            glue_ns_names = []
        for ns_name in [Name(ns.to_text()) for ns in ns_rrset.get_rdata()]:
            consistent = False
            for glue_ns_name in glue_ns_names:
                if ns_name == glue_ns_name:
                    consistent = True
                    break
            if consistent:
                continue
            rcode, rrset, _ = \
                self.__cache.find(ns_name, self.__qclass, RRType.A())
            if rrset is None and rcode is None:
                self.dprint(LOGLVL_INFO,
                            'no information for inconsistent NS name: %s',
                            [ns_name])
                self.__fetch_ns_addrs(ns_name, self.__aux_fetch_queries, False)

    def __handle_negative_answer(self, resp_msg, respinfo):
        rcode = resp_msg.get_rcode()
        if rcode == Rcode.NOERROR():
            rcode = Rcode.NXRRSET()
        neg_ttl = None
        for auth_rrset in resp_msg.get_section(Message.SECTION_AUTHORITY):
            if auth_rrset.get_class() == self.__qclass and \
                    auth_rrset.get_type() == RRType.SOA():
                cmp_result = auth_rrset.get_name().compare(self.__qname)
                cmp_reln = cmp_result.get_relation()
                if cmp_reln != NameComparisonResult.EQUAL and \
                        cmp_reln != NameComparisonResult.SUPERDOMAIN:
                    self.dprint(LOGLVL_INFO, 'bogus SOA name for negative: %s',
                                [auth_rrset.get_name()])
                    continue
                self.__cache.add(auth_rrset, SimpleDNSCache.TRUST_ANSWER,
                                 (0, SimpleDNSCache.RESP_FINAL_ANSWER_NONE))
                neg_ttl = get_soa_ttl(auth_rrset.get_rdata()[0])
                self.dprint(LOGLVL_DEBUG10,
                            'got a negative response, code=%s, negTTL=%s',
                            [rcode, neg_ttl])
                break      # Ignore any other records once we find SOA

        if neg_ttl is None:
            self.dprint(LOGLVL_DEBUG1,
                        'negative answer, code=%s, (missing SOA)', [rcode])
            neg_ttl = self.DEFAULT_NEGATIVE_TTL
        neg_rrset = RRset(self.__qname, self.__qclass, self.__qtype,
                          RRTTL(neg_ttl))
        self.__cache.add(neg_rrset, SimpleDNSCache.TRUST_ANSWER, respinfo,
                         rcode)

    def __handle_referral(self, resp_msg, ns_rrset, respinfo):
        self.dprint(LOGLVL_DEBUG10, 'got a referral: %s', [ns_rrset])
        self.__cache.add(ns_rrset, SimpleDNSCache.TRUST_GLUE, respinfo)
        additionals = resp_msg.get_section(Message.SECTION_ADDITIONAL)
        for ad_rrset in additionals:
            cmp_reln = \
                self.__cur_zone.compare(ad_rrset.get_name()).get_relation()
            if cmp_reln != NameComparisonResult.EQUAL and \
                    cmp_reln != NameComparisonResult.SUPERDOMAIN:
                self.dprint(LOGLVL_DEBUG10,
                            'ignore out-of-zone additional: %s', [ad_rrset])
                continue
            if ad_rrset.get_type() == RRType.A() or \
                    ad_rrset.get_type() == RRType.AAAA():
                self.dprint(LOGLVL_DEBUG10, 'got glue for referral: %s',
                            [ad_rrset])
                self.__cache.add(ad_rrset, SimpleDNSCache.TRUST_GLUE)
        prev_zone = self.__cur_zone
        self.__cur_zone = ns_rrset.get_name()
        (self.__ns_addr4, self.__ns_addr6) = \
            self.__find_ns_addrs(ns_rrset, True, prev_zone)
        return self.__try_next_server()

    def __try_next_server(self):
        self.__cur_ns_addr = None
        ns_addr = None
        if self.__sock4 and len(self.__ns_addr4) > 0:
            ns_addr, self.__ns_addr4 = self.__ns_addr4[0], self.__ns_addr4[1:]
        elif self.__sock6 and len(self.__ns_addr6) > 0:
            ns_addr, self.__ns_addr6 = self.__ns_addr6[0], self.__ns_addr6[1:]
        if ns_addr is None:
            return None

        # create a new query, replacing QID
        qid = None
        for i in range(0, 10):  # heuristics: try up to 10 times to generate it
            qid = random.randint(0, 65535)
            if not (qid, ns_addr) in self.__qtable:
                break
        if qid is None:
            raise MiniResolverException('failed to find unique QID')
        self.__qid = qid
        # Block this combination to avoid collision.  This will be filled in
        # later
        self.__qtable[(qid, ns_addr)] = True
        self.__msg.set_qid(self.__qid)
        self.__renderer.clear()
        self.__msg.to_wire(self.__renderer)
        qdata = self.__renderer.get_data()

        if len(ns_addr) == 2:   # should be IPv4 socket address
            self.__sock4.sendto(qdata, ns_addr)
        else:
            self.__sock6.sendto(qdata, ns_addr)
        self.__cur_ns_addr = ns_addr
        self.dprint(LOGLVL_DEBUG10, 'sent query, QID=%s', [self.__qid])
        return ns_addr

    def __find_deepest_nameserver(self, qname):
        '''Find NS RRset for the deepest known zone toward the qname.

        In this simple implementation information for the root zone is always
        available, so the search should always succeed.
        '''
        zname = qname
        ns_rrset = None
        for l in range(0, zname.get_labelcount()):
            zname = qname.split(l)
            _, ns_rrset, _ = \
                self.__cache.find(zname, self.__qclass, RRType.NS(),
                                  SimpleDNSCache.FIND_ALLOW_NOANSWER)
            if ns_rrset is not None:
                return zname, ns_rrset
        raise MiniResolverException('no name server found for ' + str(qname))

    def __find_ns_addrs(self, nameservers, fetch_if_notfound,
                        delegating_zone=None):
        v4_addrs = []
        v6_addrs = []
        rcode4 = None
        rcode6 = None
        ns_names = []
        ns_class = nameservers.get_class()
        for ns in nameservers.get_rdata():
            ns_name = Name(ns.to_text())
            ns_names.append(ns_name)
            find_option = SimpleDNSCache.FIND_ALLOW_NOANSWER
            if delegating_zone is not None and fetch_if_notfound:
                cmp_reln = delegating_zone.compare(ns_name).get_relation()
                if (cmp_reln != NameComparisonResult.EQUAL and
                    cmp_reln != NameComparisonResult.SUPERDOMAIN):
                    # out-of-zone glue.  fetch real answer if it doesn't exist.
                    find_option = SimpleDNSCache.FIND_DEFAULT
            if self.__sock4:
                rcode4, rrset4, _ = \
                    self.__cache.find(ns_name, ns_class, RRType.A(),
                                      find_option)
                if rrset4 is not None:
                    for rdata in rrset4.get_rdata():
                        v4_addrs.append((rdata.to_text(), DNS_PORT))
            if self.__sock6:
                rcode6, rrset6, _ = \
                    self.__cache.find(ns_name, ns_class, RRType.AAAA(),
                                      find_option)
                if rrset6 is not None:
                    for rdata in rrset6.get_rdata():
                        # specify 0 for flowinfo and scopeid unconditionally
                        v6_addrs.append((rdata.to_text(), DNS_PORT, 0, 0))

        # If necessary and required, invoke NS-fetch queries.  If rcodeN is not
        # None, we know the corresponding AAAA/A is not available (either
        # due to server failure or because records don't exist), in which case
        # we don't bother to fetch them.
        if (fetch_if_notfound and not v4_addrs and not v6_addrs and
            rcode4 is None and rcode6 is None):
            self.dprint(LOGLVL_DEBUG5,
                        'no address found for any nameservers')
            if self.__nest > self.FETCH_DEPTH_MAX:
                self.dprint(LOGLVL_INFO, 'reached fetch depth limit')
            else:
                self.__cur_nameservers = nameservers
                for ns in ns_names:
                    self.__fetch_ns_addrs(ns, self.__fetch_queries)
        return (v4_addrs, v6_addrs)

    def __fetch_ns_addrs(self, ns_name, fetch_queries, set_parent=True):
        for type in [RRType.A(), RRType.AAAA()]:
            res_ctx = ResolverContext(self.__sock4, self.__sock6,
                                      self.__renderer, ns_name, self.__qclass,
                                      type, self.__cache, self.__qtable,
                                      self.__stat, self.__nest + 1)
            res_ctx.set_debug_level(self.__debug_level)
            if set_parent:
                res_ctx.__parent = self
            (qid, ns_addr) = res_ctx.start()
            query = ResQuery(res_ctx, qid, ns_addr)
            fetch_queries.add(query)

    def __resume_parents(self):
        ctx = self
        while not ctx.__fetch_queries and ctx.__parent is not None:
            resumed, next_qry = ctx.__parent.__resume(ctx)
            if next_qry is not None:
                return next_qry
            if not resumed:
                # this parent is still waiting for some queries.  We're done
                # for now.
                return None

            # There's no more hope for completing the parent context.
            # Cache SERVFAIL.
            ctx.__parent.dprint(LOGLVL_DEBUG1, 'resumed context failed')
            fail_rrset = RRset(ctx.__parent.__qname, ctx.__parent.__qclass,
                               ctx.__parent.__qtype, RRTTL(self.SERVFAIL_TTL))
            self.__cache.add(fail_rrset, SimpleDNSCache.TRUST_ANSWER,
                             (0, SimpleDNSCache.RESP_FINAL_ANSWER_NONE),
                             Rcode.SERVFAIL())
            # Recursively check grand parents
            ctx = ctx.__parent
        return None

    def __resume(self, fetch_ctx):
        # Resume the parent if the current context completes the last
        # outstanding fetch query.
        # Return (bool, ResQuery): boolean is true/false if the parent is
        #   actually resumed/still suspended, respectively; ResQuery is not
        #   None iff the parrent is resumed and restarts a new query.
        for qry in self.__fetch_queries:
            if qry.res_ctx == fetch_ctx:
                self.__fetch_queries.remove(qry)
                if not self.__fetch_queries:
                    # all fetch queries done, continue the original context
                    self.dprint(LOGLVL_DEBUG10, 'resumed')
                    (self.__ns_addr4, self.__ns_addr6) = \
                        self.__find_ns_addrs(self.__cur_nameservers, False)
                    ns_addr = self.__try_next_server()
                    if ns_addr is not None:
                        return True, ResQuery(self, self.__qid, ns_addr)
                    else:
                        return True, None
                else:
                    return False, None # still waiting for some queries
        raise MiniResolverException('unexpected case: fetch query not found')

class TimerQueue:
    '''A simple timer management queue'''
    ITEM_REMOVED = '<removed entry>'
    # This is a monotonically incremented counter to make sure two different
    # timer entries are always differentiated without comparing actual items
    # (which may not always be possible)
    counter = 0

    def __init__(self):
        self.__timerq = []      # use this as a heap
        self.__index_map = {}   # reverse map from entry to timer index

    def add(self, expire, item):
        '''Add a timer entry.

        expire (float): absolute expiration time.
        item: any object that should have timeout() method.
        '''
        entry = [expire, item]
        heapq.heappush(self.__timerq, entry)
        self.__index_map[item] = entry

    def remove(self, item):
        entry = self.__index_map.pop(item)
        entry[-1] = self.ITEM_REMOVED

    def get_current_expiration(self):
        while self.__timerq:
            cur = self.__timerq[0]
            if cur[-1] == self.ITEM_REMOVED:
                heapq.heappop(self.__timerq)
            else:
                return cur[0]
        return None

    def get_expired(self, now):
        expired_items = []
        while self.__timerq:
            cur = self.__timerq[0]
            if cur[-1] == self.ITEM_REMOVED:
                heapq.heappop(self.__timerq)
            elif now >= cur[0]:
                self.__index_map.pop(cur[-1])
                expired_entry = heapq.heappop(self.__timerq)
                expired_items.append(expired_entry[-1])
            else:
                break
        return expired_items

class QueryTimer:
    '''A timer item for a separate external query.'''
    def __init__(self, resolver, res_qry):
        self.__resolver = resolver
        self.__res_qry = res_qry
        if self.__res_qry is not None:
            self.__res_qry.timer = self

    def timeout(self):
        self.__resolver._qry_timeout(self.__res_qry)

class FileResolver:
    # <#queries>/<qclass>/<qtype>/<qname>
    RE_QUERYLINE = re.compile(r'^\d*/([^/]*)/([^/]*)/(.*)$')

    def __init__(self, query_file, options):
        # prepare cache, install root hints
        self.__cache = SimpleDNSCache()
        install_root_hint(self.__cache)

        # Open query sockets
        use_ipv6 = not options.ipv4_only
        use_ipv4 = not options.ipv6_only
        self.__select_socks = []
        if use_ipv4:
            self.__sock4 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM,
                                         socket.IPPROTO_UDP)
            self.__select_socks.append(self.__sock4)
        else:
            self.__sock4 = None
        if use_ipv6:
            self.__sock6 = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM,
                                         socket.IPPROTO_UDP)
            self.__select_socks.append(self.__sock6)
        else:
            self.__sock6 = None

        self.__shutdown = False

        # Create shared resource
        self.__renderer = MessageRenderer()
        self.__msg = Message(Message.PARSE)
        self.__query_table = {}
        self.__timerq = TimerQueue()
        self.__log_level = int(options.log_level)
        self.__res_ctxs = set()
        self.__qfile = open(query_file, 'r')
        self.__max_ctxts = int(options.max_query)
        self.__dump_file = options.dump_file
        self.__serialize_file = options.serialize_file
        self.__stat = ResolverStatistics()
        self.__stat_file = options.stat_file

        ResQuery.QUERY_TIMEOUT = int(options.query_timeo)

    def dprint(self, level, msg, params=[]):
        '''Dump a debug/log message.'''
        if self.__log_level < level:
            return
        date_time = str(datetime.datetime.today())
        sys.stdout.write(('%s ' + msg + '\n') %
                         tuple([date_time] + [str(p) for p in params]))

    def shutdown(self):
        self.dprint(LOGLVL_INFO, 'resolver shutting down')
        self.__shutdown = True

    def __check_status(self):
        if self.__shutdown:
            return False
        for i in range(len(self.__res_ctxs), self.__max_ctxts):
            res_ctx = self.__get_next_query()
            if res_ctx is None:
                break
            self.__start_resolution(res_ctx)
        return len(self.__res_ctxs) > 0

    def __start_resolution(self, res_ctx):
        res_ctx.set_debug_level(self.__log_level)
        (qid, addr) = res_ctx.start()
        res_qry = ResQuery(res_ctx, qid, addr)
        self.__res_ctxs.add(res_ctx)
        self.__query_table[(qid, addr)] = res_qry
        self.__timerq.add(res_qry.expire, QueryTimer(self, res_qry))

    def __get_next_query(self):
        line = self.__qfile.readline()
        if not line:
            return None
        m = re.match(self.RE_QUERYLINE, line)
        if not m:
            sys.stderr.write('unexpected query line: %s', line)
            return None
        qclass = RRClass(m.group(1))
        qtype = RRType(m.group(2))
        qname = Name(m.group(3))
        return ResolverContext(self.__sock4, self.__sock6, self.__renderer,
                               qname, qclass, qtype, self.__cache,
                               self.__query_table, self.__stat)

    def run(self):
        while self.__check_status():
            expire = self.__timerq.get_current_expiration()
            if expire is not None:
                now = time.time()
                timo = expire - now if expire > now else 0
            else:
                timo = None
            try:
                (r, _, _) = select.select(self.__select_socks, [], [], timo)
            except select.error as ex:
                self.dprint(LOGLVL_INFO, 'select failure: %s', [ex])
                if ex.args[0] == errno.EINTR:
                    continue
            if not r:
                # timeout
                now = time.time()
                for timo_item in self.__timerq.get_expired(now):
                    timo_item.timeout()
                continue
            ready_socks = []
            if self.__sock4 in r:
                ready_socks.append(self.__sock4)
            if self.__sock6 in r:
                ready_socks.append(self.__sock6)
            for s in ready_socks:
                self.__handle(s)

    def done(self):
        if self.__dump_file is not None:
            self.__cache.dump(self.__dump_file)
        if self.__serialize_file is not None:
            self.__cache.dump(self.__serialize_file, True)
        if self.__stat_file is not None:
            self.__stat_dump()

    def __handle(self, s):
        pkt, remote = s.recvfrom(4096)
        self.__msg.clear(Message.PARSE)
        try:
            self.__msg.from_wire(pkt)
        except Exception as ex:
            self.dprint(LOGLVL_INFO, 'broken packet from %s: %s',
                        [remote[0], ex])
            self.__stat.response_broken += 1
            return
        self.dprint(LOGLVL_DEBUG10, 'received packet from %s, QID=%s',
                    [remote[0], self.__msg.get_qid()])
        res_qry = self.__query_table.get((self.__msg.get_qid(), remote))
        if res_qry is not None:
            self.__timerq.remove(res_qry.timer) # should not be None
            del self.__query_table[(self.__msg.get_qid(), remote)]
            ctx = res_qry.res_ctx
            try:
                self.__res_ctxs.remove(ctx) # maybe re-inserted below
            except KeyError as ex:
                ctx.dprint(LOGLVL_INFO, 'bug: missing context')
                raise ex
            res_qry = ctx.handle_response(self.__msg, pkt)
            next_queries = [] if res_qry is None else [res_qry]
            next_queries.extend(ctx.get_aux_queries())
            for res_qry in next_queries:
                self.__res_ctxs.add(res_qry.res_ctx)
                self.__query_table[(res_qry.qid, res_qry.ns_addr)] = res_qry
                self.__timerq.add(res_qry.expire, QueryTimer(self, res_qry))
            if not ctx in self.__res_ctxs:
                ctx.dprint(LOGLVL_DEBUG1,
                           'resolution completed, remaining ctx=%s',
                           [len(self.__res_ctxs)])
        else:
            self.dprint(LOGLVL_INFO, 'unknown response from %s, QID=%s',
                        [remote[0], self.__msg.get_qid()])

    def _qry_timeout(self, res_qry):
        self.__stat.query_timeout += 1
        del self.__query_table[(res_qry.qid, res_qry.ns_addr)]
        next_res_qry = res_qry.res_ctx.query_timeout(res_qry.ns_addr)
        if next_res_qry is None or next_res_qry.res_ctx != res_qry.res_ctx:
            # the current context is completed.  remove it from the queue.
            res_qry.res_ctx.dprint(LOGLVL_DEBUG1,
                                   'resolution timeout, remaining ctx=%s',
                                   [len(self.__res_ctxs)])
            self.__res_ctxs.remove(res_qry.res_ctx)
        if next_res_qry is not None:
            if next_res_qry.res_ctx != res_qry.res_ctx:
                # context has been replaced.  push it to the queue
                self.__res_ctxs.add(next_res_qry.res_ctx)
            self.__query_table[(next_res_qry.qid, next_res_qry.ns_addr)] = \
                next_res_qry
            timer = QueryTimer(self, next_res_qry)
            self.__timerq.add(next_res_qry.expire, timer)

    def __stat_dump(self):
        with open(self.__stat_file, 'w') as f:
            self.__stat.dump(f)

def get_option_parser():
    parser = OptionParser(usage='usage: %prog [options] query_file')
    parser.add_option("-6", "--ipv6-only", dest="ipv6_only",
                      action="store_true", default=False,
                      help="Use IPv6 transport only (disable IPv4)")
    parser.add_option("-4", "--ipv4-only", dest="ipv4_only",
                      action="store_true", default=False,
                      help="Use IPv4 transport only (disable IPv6)")
    parser.add_option("-d", "--log-level", dest="log_level", action="store",
                      default=0,
                      help="specify the log level of main resolver")
    parser.add_option("-f", "--dump-file", dest="dump_file", action="store",
                      default=None,
                      help="if specified, file name to dump the resulting " + \
                          "cache in text format")
    parser.add_option("-s", "--serialize", dest="serialize_file",
                      action="store", default=None,
                      help="if specified, file name to dump the resulting " + \
                          "cache in the serialized binary format")
    parser.add_option("-S", "--dump-stat", dest="stat_file",
                      action="store", default=None,
                      help="if specified, file to dump statistics")
    parser.add_option("-n", "--max-query", dest="max_query", action="store",
                      default="10",
                      help="specify the max # of queries in parallel")
    parser.add_option("-t", "--query-timeout", dest="query_timeo",
                      action="store", default="60",
                      help="specify query timeout in seconds")
    return parser

if __name__ == '__main__':
    parser = get_option_parser()
    (options, args) = parser.parse_args()

    if len(args) == 0:
        parser.error('query file is missing')

    resolver = FileResolver(args[0], options)

    signal.signal(signal.SIGINT, lambda sig, frame: resolver.shutdown())
    signal.signal(signal.SIGTERM, lambda sig, frame: resolver.shutdown())

    resolver.run()
    resolver.done()
