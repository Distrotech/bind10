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
from dns_cache import SimpleDNSCache, install_root_hint
import datetime
import heapq
import random
import sys
import socket
import select
import time

DNS_PORT = 53

random.random()

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

class ResolverContext:
    CNAME_LOOP_MAX = 15

    LOGLVL_INFO = 0
    LOGLVL_DEBUG1 = 1
    LOGLVL_DEBUG3 = 3    # rare event, but can happen in real world
    LOGLVL_DEBUG5 = 5           # some unexpected event, but sometimes happen
    LOGLVL_DEBUG10 = 10         # detailed trace

    def __init__(self, sock4, sock6, renderer, qname, qclass, qtype, cache,
                 nest=0):
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
        self.__debug_level = self.LOGLVL_INFO
        self.__fetch_queries = set()
        self.__parent = None    # set for internal fetch contexts
        self.__cur_zone = None
        self.__cur_ns_addr = None

    def set_debug_level(self, level):
        self.__debug_level = level

    def __dprint(self, level, msg, params=[]):
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
        sys.stdout.write(('%s ' + msg + ' %s\n') %
                         tuple([date_time] + [str(p) for p in params] +
                               [postfix]))

    def get_aux_queries(self):
        return list(self.__fetch_queries)

    def __create_query(self):
        '''Create a template query.  QID will be filled on send.'''
        self.__msg.clear(Message.RENDER)
        self.__msg.set_opcode(Opcode.QUERY())
        self.__msg.set_rcode(Rcode.NOERROR())
        self.__msg.add_question(Question(self.__qname, self.__qclass,
                                         self.__qtype))

    def start(self):
        # identify the deepest zone cut
        self.__cur_zone, nameservers = self.__find_deepest_nameserver()
        self.__dprint(self.LOGLVL_DEBUG10, 'Located deepest zone cut')

        # get server addresses
        (self.__ns_addr4, self.__ns_addr6) = self.__find_ns_addrs(nameservers)
        cur_ns_addr = self.__try_next_server()
        if cur_ns_addr is not None:
            return self.__qid, cur_ns_addr
        return None, None

    def query_timeout(self, ns_addr):
        self.__dprint(self.LOGLVL_DEBUG5, 'query timeout')
        cur_ns_addr = self.__try_next_server()
        if cur_ns_addr is not None:
            return self.__qid, cur_ns_addr
        self.__dprint(self.LOGLVL_DEBUG1, 'no reachable server')
        return None, None

    def handle_response(self, resp_msg):
        # Minimal validation
        if resp_msg.get_rr_count(Message.SECTION_QUESTION) != 1:
            sys.stderr.write('unexpected # of question in response\n')
        question = resp_msg.get_question()[0]
        if question.get_name() != self.__qname or \
                question.get_class() != self.__qclass or \
                question.get_type() != self.__qtype:
            sys.stderr.write('unexpected response: query mismatch')
        if resp_msg.get_qid() != self.__qid:
            sys.stderr.write('unexpected response: QID mismatch')

        # Look into the response
        next_qry = None
        try:
            if resp_msg.get_header_flag(Message.HEADERFLAG_AA):
                next_qry = self.__handle_auth_answer(resp_msg)
            elif resp_msg.get_rcode() == Rcode.NOERROR() and \
                    (not resp_msg.get_header_flag(Message.HEADERFLAG_AA)):
                authorities = resp_msg.get_section(Message.SECTION_AUTHORITY)
                new_zone = None
                for ns_rrset in authorities:
                    if ns_rrset.get_type() == RRType.NS():
                        ns_addr = self.__handle_referral(resp_msg, ns_rrset)
                        if ns_addr is not None:
                            next_qry = ResQuery(self, self.__qid, ns_addr)
                        break
            else:
                raise InternalLame('lame server')
        except InternalLame as ex:
            self.__dprint(self.LOGLVL_INFO, '%s', [ex])
            ns_addr = self.__try_next_server()
            if ns_addr is not None:
                next_qry = ResQuery(self, self.__qid, ns_addr)
            else:
                self.__dprint(self.LOGLVL_DEBUG1, 'no usable server')
        if next_qry is None and self.__parent is not None:
            # This context is completed, resume the parent
            next_qry = self.__parent.__resume(self)
        return next_qry

    def __handle_auth_answer(self, resp_msg):
        '''Subroutine of handle_response, handling an authoritative answer.'''
        if resp_msg.get_rcode() == Rcode.NOERROR() and \
            resp_msg.get_rr_count(Message.SECTION_ANSWER) > 0:
            answer_rrset = resp_msg.get_section(Message.SECTION_ANSWER)[0]
            if answer_rrset.get_name() == self.__qname and \
                    answer_rrset.get_class() == self.__qclass:
                self.__cache.add(answer_rrset, SimpleDNSCache.TRUST_ANSWER)
                if answer_rrset.get_type() == self.__qtype:
                    self.__dprint(self.LOGLVL_DEBUG10, 'got a response: %s',
                                  [answer_rrset])
                    return None
                elif answer_rrset.get_type() == RRType.CNAME():
                    self.__dprint(self.LOGLVL_DEBUG10, 'got an alias: %s',
                                  [answer_rrset])
                    # Chase CNAME with a separate resolver context with loop
                    # prevention
                    if self.__nest > self.CNAME_LOOP_MAX:
                        self.__dprint(self.LOGLVL_INFO, 'possible CNAME loop')
                        return None
                    if self.__parent is not None:
                        # Don't chase CNAME in an internal fetch context
                        self.__dprint(self.LOGLVL_INFO,
                                      'CNAME in internal fetch')
                        return None
                    cname = Name(answer_rrset.get_rdata()[0].to_text())
                    cname_ctx = ResolverContext(self.__sock4, self.__sock6,
                                                self.__renderer, cname,
                                                self.__qclass, self.__qtype,
                                                self.__cache, self.__nest + 1)
                    cname_ctx.set_debug_level(self.__debug_level)
                    (qid, ns_addr) = cname_ctx.start()
                    if ns_addr is not None:
                        return ResQuery(cname_ctx, qid, ns_addr)
                    return None
        elif resp_msg.get_rcode() == Rcode.NXDOMAIN() or \
                (resp_msg.get_rcode() == Rcode.NOERROR() and
                 resp_msg.get_rr_count(Message.SECTION_ANSWER) == 0):
            self.__handle_negative_answer(resp_msg)
            return None

        raise InternalLame('unexpected answer')

    def __handle_negative_answer(self, resp_msg):
        rcode = resp_msg.get_rcode()
        if rcode == Rcode.NOERROR():
            rcode = Rcode.NXRRSET()
        for auth_rrset in resp_msg.get_section(Message.SECTION_AUTHORITY):
            if auth_rrset.get_class() == self.__qclass and \
                    auth_rrset.get_type() == RRType.SOA():
                cmp_result = auth_rrset.get_name().compare(self.__qname)
                cmp_reln = cmp_result.get_relation()
                if cmp_reln == NameComparisonResult.EQUAL or \
                        cmp_reln == NameComparisonResult.SUPERDOMAIN:
                    neg_ttl = get_soa_ttl(auth_rrset.get_rdata()[0])
                self.__dprint(self.LOGLVL_DEBUG10,
                              'got a negative response, code=%s, negTTL=%s',
                              [rcode, neg_ttl])
                neg_rrset = RRset(self.__qname, self.__qclass, self.__qtype,
                                  RRTTL(neg_ttl))
                self.__cache.add(neg_rrset, SimpleDNSCache.TRUST_ANSWER, rcode)

                # Ignore any other records once we find SOA
                return
        delf.dprint(self.LOGLVL_INFO,
                    'unexpected negative answer (missing SOA)')

    def __handle_referral(self, resp_msg, ns_rrset):
        self.__dprint(self.LOGLVL_DEBUG10, 'got a referral: %s', [ns_rrset])
        self.__cache.add(ns_rrset, SimpleDNSCache.TRUST_GLUE)
        self.__cur_zone = ns_rrset.get_name()
        additionals = resp_msg.get_section(Message.SECTION_ADDITIONAL)
        for ad_rrset in additionals:
            cmp_reln = \
                self.__cur_zone.compare(ad_rrset.get_name()).get_relation()
            if cmp_reln != NameComparisonResult.EQUAL and \
                    cmp_reln != NameComparisonResult.SUPERDOMAIN:
                self.__dprint(self.LOGLVL_DEBUG10,
                              'ignore out-of-zone additional: %s', [ad_rrset])
                continue
            if ad_rrset.get_type() == RRType.A() or \
                    ad_rrset.get_type() == RRType.AAAA():
                self.__dprint(self.LOGLVL_DEBUG10, 'got glue for referral:%s',
                              [ad_rrset])
                self.__cache.add(ad_rrset, SimpleDNSCache.TRUST_GLUE)
        (self.__ns_addr4, self.__ns_addr6) = self.__find_ns_addrs(ns_rrset)
        return self.__try_next_server()

    def __try_next_server(self):
        self.__cur_ns_addr = None
        ns_addr = None
        if len(self.__ns_addr4) > 0:
            ns_addr, self.__ns_addr4 = self.__ns_addr4[0], self.__ns_addr4[1:]
        elif len(self.__ns_addr6) > 0:
            ns_addr, self.__ns_addr6 = self.__ns_addr6[0], self.__ns_addr6[1:]
        if ns_addr is None:
            return None

        # create a new query, replacing QID
        self.__qid = random.randint(0, 65535)
        self.__msg.set_qid(self.__qid)
        self.__renderer.clear()
        self.__msg.to_wire(self.__renderer)
        qdata = self.__renderer.get_data()

        if len(ns_addr) == 2:   # should be IPv4 socket address
            self.__sock4.sendto(qdata, ns_addr)
        else:
            self.__sock6.sendto(qdata, ns_addr)
        self.__cur_ns_addr = ns_addr
        self.__dprint(self.LOGLVL_DEBUG10, 'sent query')
        return ns_addr

    def __find_deepest_nameserver(self):
        '''Find NS RRset for the deepest known zone toward the qname.

        In this simple implementation information for the root zone is always
        available, so the search should always succeed.
        '''
        zname = self.__qname
        ns_rrset = None
        for l in range(0, zname.get_labelcount()):
            zname = self.__qname.split(l)
            ns_rrset = self.__cache.find(zname, self.__qclass, RRType.NS(),
                                         SimpleDNSCache.FIND_ALLOW_GLUE)
            if ns_rrset is not None:
                return zname, ns_rrset
        raise MiniResolverException('no name server found for ' +
                                    str(self.__qname))

    def __find_ns_addrs(self, nameservers, fetch_if_notfound=True):
        v4_addrs = []
        v6_addrs = []
        ns_names = []
        ns_class = nameservers.get_class()
        for ns in nameservers.get_rdata():
            ns_name = Name(ns.to_text())
            ns_names.append(ns_name)
            rrset4 = self.__cache.find(ns_name, ns_class, RRType.A(),
                                       SimpleDNSCache.FIND_ALLOW_GLUE)
            if rrset4 is not None:
                for rdata in rrset4.get_rdata():
                    v4_addrs.append((rdata.to_text(), DNS_PORT))
            rrset6 = self.__cache.find(ns_name, ns_class, RRType.AAAA(),
                                      SimpleDNSCache.FIND_ALLOW_GLUE)
            if rrset6 is not None:
                for rdata in rrset6.get_rdata():
                    # specify 0 for flowinfo and scopeid unconditionally
                    v6_addrs.append((rdata.to_text(), DNS_PORT, 0, 0))
        if fetch_if_notfound and not v4_addrs and not v6_addrs:
            self.__dprint(self.LOGLVL_DEBUG5,
                          'no address found for any nameservers')
            self.__cur_nameservers = nameservers
            for ns in ns_names:
                self.__fetch_ns_addrs(ns)
        return (v4_addrs, v6_addrs)

    def __fetch_ns_addrs(self, ns_name):
        for type in [RRType.A(), RRType.AAAA()]:
            res_ctx = ResolverContext(self.__sock4, self.__sock6,
                                      self.__renderer, ns_name, self.__qclass,
                                      type, self.__cache, 0)
            res_ctx.set_debug_level(self.__debug_level)
            res_ctx.__parent = self
            (qid, ns_addr) = res_ctx.start()
            if ns_addr is not None:
                query = ResQuery(res_ctx, qid, ns_addr)
                self.__fetch_queries.add(query)

    def __resume(self, fetch_ctx):
        for qry in self.__fetch_queries:
            if qry.res_ctx == fetch_ctx:
                self.__fetch_queries.remove(qry)
                if not self.__fetch_queries:
                    # all fetch queries done, continue the original context
                    (self.__ns_addr4, self.__ns_addr6) = \
                        self.__find_ns_addrs(self.__cur_nameservers, False)
                    ns_addr = self.__try_next_server()
                    if ns_addr is not None:
                        return ResQuery(self, self.__qid, ns_addr)
                    else:
                        return None
                else:
                    return None
        raise MiniResolverException('unexpected case: fetch query not found')

class TimerQueue:
    '''A simple timer management queue'''
    ITEM_REMOVED = '<removed>'

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
                expired_entry = heapq.heappop(self.__timerq)
                expired_items.append(expired_entry[1])
            else:
                break
        return expired_items

class QueryTimer:
    '''A timer item for a separate external query.'''
    def __init__(self, res_qry, timerq, qtable):
        self.__res_qry = res_qry
        self.__res_qry.timer = self
        self.__timerq = timerq
        self.__qtable = qtable

    def timeout(self):
        (qid, addr) = \
            self.__res_qry.res_ctx.query_timeout(self.__res_qry.ns_addr)
        if addr is not None:
            next_res_qry = ResQuery(self.__res_qry.res_ctx, qid, addr)
            self.__qtable[(qid, addr)] = next_res_qry
            timer = QueryTimer(next_res_qry, self.__timerq, self.__qtable)
            self.__timerq.add(next_res_qry.expire, timer)

if __name__ == '__main__':
    # prepare cache, install root hints
    cache = SimpleDNSCache()
    install_root_hint(cache)

    # Open query sockets
    sock4 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM,
                          socket.IPPROTO_UDP)
    sock6 = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM,
                          socket.IPPROTO_UDP)

    # Create reused resource
    renderer = MessageRenderer()
    msg = Message(Message.PARSE)
    qclass = RRClass.IN()

    # Sample query data
    queries = [(Name('ns.jinmei.org'), RRType.AAAA()),
               (Name('ns.jinmei.org'), RRType.TXT()), # for NXRRSET
               (Name('nxdomain.jinmei.org'), RRType.A()), # for NXDOMAIN
               (Name('www.apple.com'), RRType.A()),
               (Name('www.jinmei.org'), RRType.AAAA())] # for CNAME
    ctxs = set([ResolverContext(sock4, sock6, renderer, x[0], qclass, x[1],
                                cache) for x in queries])
    query_table = {}
    timerq = TimerQueue()
    for ctx in ctxs:
        ctx.set_debug_level(ResolverContext.LOGLVL_INFO)
        (qid, addr) = ctx.start()
        res_qry = ResQuery(ctx, qid, addr)
        query_table[(qid, addr)] = res_qry
        timerq.add(res_qry.expire, QueryTimer(res_qry, timerq, query_table))

    while len(ctxs) > 0:
        expire = timerq.get_current_expiration()
        if expire is not None:
            now = time.time()
            timo = expire - now if expire > now else 0
        else:
            timo = None
        (r, _, _) = select.select([sock4, sock6], [], [], timo)
        if not r:
            # timeout
            now = time.time()
            for timo_item in timerq.get_expired(now):
                timo_item.timeout()
            continue
        ready_socks = []
        if sock4 in r:
            ready_socks.append(sock4)
        if sock6 in r:
            ready_socks.append(sock6)
        for s in ready_socks:
            pkt, remote = s.recvfrom(4096)
            msg.clear(Message.PARSE)
            msg.from_wire(pkt)
            res_qry = query_table.get((msg.get_qid(), remote))
            timerq.remove(res_qry.timer) # should not be None
            if res_qry is not None:
                del query_table[(msg.get_qid(), remote)]
                ctx = res_qry.res_ctx
                ctxs.remove(ctx) # maybe re-inserted below
                res_qry = ctx.handle_response(msg)
                next_queries = [] if res_qry is None else [res_qry]
                next_queries.extend(ctx.get_aux_queries())
                for res_qry in next_queries:
                    ctxs.add(res_qry.res_ctx)
                    query_table[(res_qry.qid, res_qry.ns_addr)] = res_qry
                    timerq.add(res_qry.expire, QueryTimer(res_qry, timerq,
                                                          query_table))

    for q in queries:
        rrset = cache.find(q[0], qclass, q[1],
                           SimpleDNSCache.FIND_ALLOW_NEGATIVE |
                           SimpleDNSCache.FIND_ALLOW_GLUE |
                           SimpleDNSCache.FIND_ALLOW_CNAME)
        if rrset is not None and rrset.get_rdata_count() != 0:
            print(rrset)
