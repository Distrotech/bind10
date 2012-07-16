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
import parse_qrylog
from dns_cache import SimpleDNSCache, CacheShell

import datetime
from optparse import OptionParser
import re
import sys

convert_rrtype = parse_qrylog.convert_rrtype
RE_LOGLINE = parse_qrylog.RE_LOGLINE

LOGLVL_INFO = 0
LOGLVL_DEBUG1 = 1
LOGLVL_DEBUG3 = 3    # rare event, but can happen in real world
LOGLVL_DEBUG5 = 5           # some unexpected event, but sometimes happen
LOGLVL_DEBUG10 = 10         # detailed trace

class QueryReplaceError(Exception):
    pass

class QueryTrace:
    '''consists of...

    ttl(int): TTL
    resp_size(int): estimated size of the response to the query.

    '''
    def __init__(self, ttl, cache_entries, rcode):
        self.ttl = ttl
        self.rcode = rcode
        self.__cache_entries = cache_entries
        self.cname_trace = []
        self.__cache_log = []

    def get_query_count(self):
        '''Return the total count of this query'''
        n_queries = 0
        for log in self.__cache_log:
            n_queries += log.misses + log.hits
        return n_queries

    def get_cache_hits(self):
        '''Return the total count of cache hits for the query'''
        hits = 0
        for log in self.__cache_log:
            hits += log.hits
        return hits

    def get_cache_misses(self):
        '''Return the total count of cache misses for the query'''
        misses = 0
        for log in self.__cache_log:
            misses += log.misses
        return misses

    def get_external_query_count(self):
        '''Return a list of external queries needed for cache update.

        Each list entry is an int that means the number of queries.

        '''
        counts = []
        for log in self.__cache_log:
            counts.append(len(log.resp_list))
        return counts

    def add_cache_log(self, cache_log):
        self.__cache_log.append(cache_log)

    def get_last_cache(self):
        if len(self.__cache_log) == 0:
            return None
        return self.__cache_log[-1]

    def update(self, cache, now):
        '''Update all cached records associated with the query at once.

        This is effectively only useful for type ANY query results.

        '''
        for cache_entry_id in self.__cache_entries:
            cache.update(cache_entry_id, now)

class CacheLog:
    '''consists of...

    __time_created (float): Timestamp when an answer for the query is cached.
    hits (int): number of cache hits
    misses (int): 1 if this is created on cache miss (normal); otherwise 0
    resp_list [(int, int)]: additional info on external queries needed to
      create this entry, from deepest (leaf zone) to top (known deepest
      zone cut toward the query name at the time of resolution).  Each list
      entry consists of (response_size, response type (RESP_xxx))

    '''
    def __init__(self, now, resp_list, on_miss=True):
        self.time_last_used = now
        self.hits = 0
        self.misses = 1 if on_miss else 0
        self.resp_list = resp_list

class ResolverContext:
    '''Emulated resolver context.'''
    FETCH_DEPTH_MAX = 8         # prevent infinite NS fetch
    override_mode = False       # set by QueryReplay

    def __init__(self, qname, qclass, qtype, cache, now, dbg_level, nest=0):
        self.__qname = qname
        self.__qclass = qclass
        self.__qtype = qtype
        self.__cache = cache
        self.__now = now
        self.__dbg_level = dbg_level
        self.__cur_zone = qname # sentinel
        self.__nest = nest

    def dprint(self, level, msg, params=[]):
        '''Dump a debug/log message.'''
        if self.__dbg_level < level:
            return
        date_time = str(datetime.datetime.today())
        postfix = '[%s/%s/%s at %s]' % (self.__qname.to_text(True),
                                        str(self.__qclass), str(self.__qtype),
                                        self.__cur_zone)
        sys.stdout.write(('%s ' + msg + ' %s\n') %
                         tuple([date_time] + [str(p) for p in params] +
                               [postfix]))

    def resolve(self):
        # The cache actually has the resolution result with full delegation
        # chain; it's just some part of it has expired.  We first need to
        # identify the part to be resolved.
        chain, resp_list = self.__get_resolve_chain()
        if len(chain) == 1:
            return chain[0][1], False, resp_list

        self.dprint(LOGLVL_DEBUG5, 'cache miss, emulate resolving')

        while True:
            nameservers = chain[-1][1] # deepest active NS RRset
            self.__cur_zone = nameservers.get_name()
            self.dprint(LOGLVL_DEBUG10, 'reach a zone cut')

            found_addr, n_addr, fetch_list = self.__find_ns_addrs(nameservers)
            if n_addr == 0:
                # If fetching NS addresses fails, we should be at the end of
                # chain.
                found, fetch_resps = self.__fetch_ns_addrs(fetch_list,
                                                           found_addr)
                resp_list.extend(fetch_resps)
                if not found:
                    chain = chain[:-1]
                    if len(chain) != 1:
                        raise QueryReplaceError('assumption failure')
                    break

            # "send query, then get response".  Now we can consider the
            # delegation records (NS and glue AAAA/A) one level deepr active
            # again.
            self.dprint(LOGLVL_DEBUG10, 'external query')
            chain = chain[:-1]
            if len(chain) == 1: # reached deepest zone, update the final cache
                break

            # Otherwise, go down to the zone one level lower.
            new_id, nameservers, _ = chain[-1]
            self.dprint(LOGLVL_DEBUG10, 'update NS at zone %s, trust %s',
                        [nameservers.get_name(),
                         self.__cache.get(new_id).trust])
            self.__cache.update(new_id, self.__now)
            self.__update_glue(nameservers)

        # We've reached the deepest zone (which should normally contain the
        # query name)
        self.dprint(LOGLVL_DEBUG5, 'resolution completed')
        self.__cache.update(chain[0][0], self.__now)
        self.__update_authns(chain[0][1], nameservers)
        self.__purge_glue(chain[0][1])
        return chain[0][1], True, resp_list

    def __fetch_ns_addrs(self, fetch_list, allskip_ok):
        if self.__nest > self.FETCH_DEPTH_MAX:
            self.dprint(LOGLVL_DEBUG1, 'reached fetch depth limit, aborted')
            return False, []

        self.dprint(LOGLVL_DEBUG10, 'no NS addresses are known, fetch them.')
        ret_resp_list = []
        skipped = 0
        for fetch in fetch_list:
            ns_name, addr_type = fetch[1], fetch[0]

            # First, check if we know this RR at all in the first place.
            # If it were subject to CNAME substituation, this name should be
            # excluded from fetch.
            if self.__cache.find(ns_name, self.__qclass,
                                 RRType.CNAME())[0] is not None:
                self.dprint(LOGLVL_DEBUG1, 'NS name is an alias %s/%s/%s',
                            [ns_name, self.__qclass, addr_type])
                continue

            # It could happen that in the original resolution the resolver
            # already knew some of the missing addresses as an answer (as a
            # result or side effect of prior resolution) and didn't bother to
            # try to fetch others.  In that case, our cache doesn't have any
            # information for this record (which would crash resolution
            # emulation below).
            if self.__cache.find(ns_name, self.__qclass, addr_type)[0] is None:
                skipped += 1
                self.dprint(LOGLVL_DEBUG10, 'skip fetching NS addrs %s/%s/%s',
                            [ns_name, self.__qclass, addr_type])
                continue

            res_ctx = ResolverContext(ns_name, self.__qclass, addr_type,
                                      self.__cache, self.__now,
                                      self.__dbg_level, self.__nest + 1)
            rrset, updated, resp_list = res_ctx.resolve()
            ret_resp_list.extend(resp_list)
            if not updated:
                # rare case, but this one could have been updated in the
                # middle of this fetch attempts
                self.dprint(LOGLVL_DEBUG10,
                            'NS fetch target has been self updated: %s/%s',
                            [ns_name, addr_type])
                                        
            if rrset.get_rdata_count() > 0: # positive result
                self.dprint(LOGLVL_DEBUG10,
                            'fetching an NS address succeeded for %s/%s/%s',
                            [ns_name, self.__qclass, addr_type])
                return True, ret_resp_list
            self.dprint(LOGLVL_DEBUG10,
                        'fetching an NS address failed for %s/%s/%s',
                        [ns_name, self.__qclass, addr_type])

        # Normally, We should be able to try fetching at least one of the
        # requested addrs; if not, it means internnal inconsistency.  The
        # exception is when the caller knows something *negative* about other
        # addresses and just checking if there's a hope in missing glue.
        if not allskip_ok and skipped > 0 and skipped == len(fetch_list):
            raise QueryReplaceError('assumption failure in NS fetch')

        # All attempts fail
        self.dprint(LOGLVL_DEBUG10, 'fetching an NS address failed')
        return False, ret_resp_list

    def __update_glue(self, ns_rrset):
        '''Update cached glue records for in-zone glue of given NS RRset.'''
        for ns_name in [Name(ns.to_text()) for ns in ns_rrset.get_rdata()]:
            # Exclude out-of-zone glue
            cmp_reln = self.__cur_zone.compare(ns_name).get_relation()
            if (cmp_reln != NameComparisonResult.SUPERDOMAIN and
                cmp_reln != NameComparisonResult.EQUAL):
                self.dprint(LOGLVL_DEBUG10,
                            'Exclude out-of-zone glue: %s', [ns_name])
                continue
            _, rrset, id = self.__find_delegate_info(ns_name, RRType.A())
            if id is not None:
                self.dprint(LOGLVL_DEBUG10, 'Update IPv4 glue: %s', [ns_name])
                self.__cache.update(id, self.__now)
            _, rrset, id = self.__find_delegate_info(ns_name, RRType.AAAA())
            if id is not None:
                self.dprint(LOGLVL_DEBUG10, 'Update IPv6 glue: %s', [ns_name])
                self.__cache.update(id, self.__now)

    def __update_authns(self, answer_rrset, ns_rrset):
        '''Update cached auth NS record that comes with an answer.

        We don't do this if the query is for the NS, in which case the
        authority NS is normally omitted.  Likewise, we don't do this for
        negative answers.  It's still not guaranteed the authority section
        always has the NS with the answer, but the fact that the cache has
        the information makes it quite likely (at least one server gave it
        with an answer in the original actual resolution).

        '''
        if self.__qtype == RRType.NS():
            return
        if answer_rrset.get_rdata_count() == 0:
            return

        id = self.__cache.find(ns_rrset.get_name(), self.__qclass, RRType.NS(),
                               SimpleDNSCache.FIND_ALLOW_NOANSWER,
                               SimpleDNSCache.TRUST_AUTHAUTHORITY)[2]
        if id is not None:
            self.dprint(LOGLVL_DEBUG10, 'update auth NS')
            self.__cache.update(id, self.__now)

        if self.override_mode:
            id = self.__cache.find(ns_rrset.get_name(), self.__qclass,
                                   RRType.NS(),
                                   SimpleDNSCache.FIND_ALLOW_NOANSWER,
                                   SimpleDNSCache.TRUST_GLUE)[2]
            if id is not None:
                self.dprint(LOGLVL_DEBUG10, 'purge glue NS')
                self.__cache.update(id, None)

    def __purge_glue(self, answer_rrset):
        if (not self.override_mode or
            (answer_rrset.get_type() != RRType.A() and
             answer_rrset.get_type() != RRType.AAAA())):
            return

        id = self.__cache.find(answer_rrset.get_name(), self.__qclass,
                               answer_rrset.get_type(),
                               SimpleDNSCache.FIND_ALLOW_NOANSWER,
                               SimpleDNSCache.TRUST_GLUE)[2]
        if id is not None:
            self.dprint(LOGLVL_DEBUG10, 'purge glue record due to answer')
            self.__cache.update(id, None)

    def __get_resolve_chain(self):
        chain = []
        resp_list = []
        rcode, answer_rrset, id = \
            self.__cache.find(self.__qname, self.__qclass, self.__qtype,
                              SimpleDNSCache.FIND_ALLOW_CNAME |
                              SimpleDNSCache.FIND_ALLOW_NEGATIVE)
        entry = self.__cache.get(id)
        chain.append((id, answer_rrset, entry))
        resp_list.append((entry.msglen, entry.resp_type))
        if not entry.is_expired(self.__now):
            return chain, []

        # Build full chain to the root.  parent_zones[i] will be set to the
        # parent zone for chain[i-1].
        parent_zones = []
        for l in range(0, self.__qname.get_labelcount()):
            zname = self.__qname.split(l)
            rcode, ns_rrset, id = self.__find_delegate_info(zname, RRType.NS())
            if ns_rrset is None or ns_rrset.get_rdata_count() == 0:
                # this could return a negative result.  we should simply
                # ignore this case.
                continue
            parent_zones.append(zname)
            entry = self.__cache.get(id)
            self.dprint(LOGLVL_DEBUG10, 'build resolve chain at %s, trust %s',
                        [zname, entry.trust])
            chain.append((id, ns_rrset, entry))
            resp_list.append((entry.msglen, entry.resp_type))

        # The last entry of parent_zones should be root.  Its parent should
        # be itself.
        parent_zones.append(parent_zones[-1])

        # Then find the deepest level where complete delegation information
        # is available (NS and at least one NS address are active).
        for i in range(1, len(chain)):
            entry = chain[i][2]
            zname = chain[i][1].get_name()
            if (not entry.is_expired(self.__now) and
                self.__is_glue_active(zname, parent_zones[i], entry)):
                self.dprint(LOGLVL_DEBUG10,
                            'located the deepest active delegtion to %s at %s',
                            [zname, parent_zones[i]])
                return chain[:i + 1], resp_list[:i]

        # In our setup root server should be always available.
        raise QueryReplaceError('no name server found for ' +
                                str(self.__qname))

    def __is_glue_active(self, zname, parent_zname, cache_entry):
        has_inzone_glue = False
        for ns_name in [Name(ns.to_text()) for ns in cache_entry.rdata_list]:
            cmp_reln = parent_zname.compare(ns_name).get_relation()
            if (cmp_reln == NameComparisonResult.SUPERDOMAIN or
                cmp_reln == NameComparisonResult.EQUAL):
                has_inzone_glue = True

            # If an address record is active and cached, we can start
            # the delegation from this point.
            for rrtype in [RRType.A(), RRType.AAAA()]:
                if self.__find_delegate_info(ns_name, rrtype,
                                             True)[1] is not None:
                    return True

        # We don't have any usable address record at this point.  If there's
        # at least one in-zone glue, the replay logic would assume it should
        # be usable by the replay time.  So we cannot start from this level.
        if has_inzone_glue:
            self.dprint(LOGLVL_DEBUG10,
                        'active NS is found but no usable address at %s',
                        [zname])
            return False
        return True

    def __find_ns_addrs(self, nameservers):
        found = False      # whether we know any active info about an address
        n_addrs = 0        # num of actually usable address

        # Record any missing address to be fetched.
        fetch_list = []
        for ns_name in [Name(ns.to_text()) for ns in nameservers.get_rdata()]:
            rcode, rrset4, id = self.__find_delegate_info(ns_name, RRType.A(),
                                                          True)
            if rrset4 is not None and rrset4.get_rdata_count() > 0:
                self.dprint(LOGLVL_DEBUG10, 'found %s IPv4 address for NS %s',
                            [rrset4.get_rdata_count(), ns_name])
                found = True
                n_addrs += 1
            elif rcode is not None:
                self.dprint(LOGLVL_DEBUG10,
                            'IPv4 address for NS %s is known to be unusable',
                            [ns_name])
                found = True
            else:
                fetch_list.append((RRType.A(), ns_name))

            rcode, rrset6, id = \
                self.__find_delegate_info(ns_name, RRType.AAAA(), True)
            if rrset6 is not None and rrset6.get_rdata_count() > 0:
                self.dprint(LOGLVL_DEBUG10, 'found %s IPv6 address for NS %s',
                            [rrset6.get_rdata_count(), ns_name])
                found = True
                n_addrs += 1
            elif rcode is not None:
                self.dprint(LOGLVL_DEBUG10,
                            'IPv6 address for NS %s is known to be unusable',
                            [ns_name])
                found = True
            else:
                fetch_list.append((RRType.AAAA(), ns_name))

        return found, n_addrs, fetch_list

    def __find_delegate_info(self, name, rrtype, active_only=False):
        '''Find an RRset from the cache that can be used for delegation.

        This is for NS and glue AAAA and As.  If active_only is True,
        expired RRsets won't be returned.

        '''
        # We first try an "answer" that has higher trust level and see
        # if it's cached and still alive.
        # If it's cached, either positive or negative, rcode should be !None.
        # We use that result in that case
        options = 0
        ans_rcode, ans_rrset, ans_id = \
            self.__cache.find(name, self.__qclass, rrtype,
                              SimpleDNSCache.FIND_ALLOW_NEGATIVE)
        if (ans_rcode is not None and
            not self.__cache.get(ans_id).is_expired(self.__now)):
            return ans_rcode, ans_rrset, ans_id

        # Next, if the requested type is NS, we consider a cached RRset at
        # the "auth authority" trust level. In this case checking rcode is
        # meaningless, so we check rrset (the result should be the same).
        # We use it only if it's active.
        if rrtype == RRType.NS():
            rcode, rrset, id = \
                self.__cache.find(name, self.__qclass, rrtype,
                                  SimpleDNSCache.FIND_ALLOW_NOANSWER,
                                  SimpleDNSCache.TRUST_AUTHAUTHORITY)
            if (rrset is not None and
                not self.__cache.get(id).is_expired(self.__now)):
                return rcode, rrset, id

        # Finally try delegation or glues.  checking rcode is meaningless
        # like the previous case.
        # Unlike other cases, we use it whether it's active or expired unless
        # explicitly requested to exclude expired ones.
        rcode, rrset, id = \
            self.__cache.find(name, self.__qclass, rrtype,
                              SimpleDNSCache.FIND_ALLOW_NOANSWER,
                              SimpleDNSCache.TRUST_GLUE)
        if (rrset is not None and
            (not active_only or
             not self.__cache.get(id).is_expired(self.__now))):
            return rcode, rrset, id

        return None, None, None

class QueryReplay:
    CACHE_OPTIONS = SimpleDNSCache.FIND_ALLOW_CNAME | \
        SimpleDNSCache.FIND_ALLOW_NEGATIVE

    def __init__(self, log_file, cache, dbg_level):
        self.__log_file = log_file
        self.__cache = cache
        # Replay result.
        # dict from (Name, RR type, RR class) to QueryTrace
        self.__queries = {}
        self.__total_queries = 0
        self.__query_params = None
        self.__override = options.override
        self.__cur_query = None # use for debug out
        self.__dbg_level = int(dbg_level)
        self.__resp_msg = Message(Message.RENDER) # for resp size estimation
        self.__renderer = MessageRenderer()
        self.__rcode_stat = {}  # RCODE value (int) => query counter
        self.__qtype_stat = {} # RR type => query counter
        self.__extqry1_stat = {} # #-of-extqry for creation => counter
        self.__extqry_update_stat = {} # #-of-extqry for update => counter
        self.__extqry_total_stat = {} # total count of the above two (shortcut)
        self.__extresp_stat = {} # RESP_xxx => counter
        self.__extresp_stat[0] = 0
        for resptype in SimpleDNSCache.RESP_DESCRIPTION.keys():
            self.__extresp_stat[resptype] = 0
        self.__interactive = options.interactive
        # Session wide constant for all resolver context instances
        ResolverContext.override_mode = options.override
        self.cache_samettl_hits = 0 # #-of cache hits that happen within 1s
        self.cache_total_hits = 0   # #-of total cache hits (shortcut)

    def dprint(self, level, msg, params=[]):
        '''Dump a debug/log message.'''
        if self.__dbg_level < level:
            return
        date_time = str(datetime.datetime.today())
        postfix = ''
        if self.__cur_query is not None:
            postfix = ' [%s/%s/%s]' % \
                (self.__cur_query[0], self.__cur_query[1], self.__cur_query[2])
        sys.stdout.write(('%s ' + msg + '%s\n') %
                         tuple([date_time] + [str(p) for p in params] +
                               [postfix]))

    def replay(self):
        with open(self.__log_file) as f:
            for log_line in f:
                self.__total_queries += 1
                try:
                    self.__replay_query(log_line)
                except Exception as ex:
                    self.dprint(LOGLVL_INFO,
                                'error (%s) at line: %s', [ex, log_line])
                    if self.__interactive:
                        CacheShell(self.__cache).cmdloop()
                    raise ex
        return self.__total_queries, len(self.__queries)

    def __replay_query(self, log_line):
        '''Replay a single query.'''
        m = re.match(RE_LOGLINE, log_line)
        if not m:
            self.dprint(LOGLVL_INFO, 'unexpected line: %s', [log_line])
            return
        qry_time = float(m.group(1))
        client_addr = m.group(2)
        qry_name = Name(m.group(3))
        qry_class = RRClass(m.group(4))
        qry_type = RRType(convert_rrtype(m.group(5)))
        qry_key = (qry_name, qry_type, qry_class)

        self.__cur_query = (qry_name, qry_class, qry_type)

        if not qry_type in self.__qtype_stat:
            self.__qtype_stat[qry_type] = 0
        self.__qtype_stat[qry_type] += 1

        qinfo = self.__queries.get(qry_key)
        if qinfo is None:
            qinfo, rrsets = \
                self.__get_query_trace(qry_name, qry_class, qry_type)
            qinfo.resp_size = self.__calc_resp_size(qry_name, rrsets)
            self.__queries[qry_key] = qinfo
        if not qinfo.rcode.get_code() in self.__rcode_stat:
            self.__rcode_stat[qinfo.rcode.get_code()] = 0
        self.__rcode_stat[qinfo.rcode.get_code()] += 1
        expired, resp_list = \
            self.__check_expired(qinfo, qry_name, qry_class, qry_type,
                                 qry_time)
        if expired:
            self.dprint(LOGLVL_DEBUG3,
                        'cache miss, updated with %s messages (%s)',
                        [len(resp_list), resp_list])
            cache_log = CacheLog(qry_time, resp_list)
            qinfo.add_cache_log(cache_log)
            self.__update_extqry_stat(qinfo, resp_list)
            self.__update_extresp_stat(resp_list)
        else:
            self.dprint(LOGLVL_DEBUG3, 'cache hit')
            cache_log = qinfo.get_last_cache()
            if cache_log is None:
                cache_log = CacheLog(qry_time, [], False)
                qinfo.add_cache_log(cache_log)
            else:
                if int(cache_log.time_last_used) == int(qry_time):
                    self.cache_samettl_hits += 1
                cache_log.time_last_used = qry_time
            cache_log.hits += 1
            self.cache_total_hits += 1

    def __check_expired(self, qinfo, qname, qclass, qtype, now):
        if qtype == RRType.ANY():
            return self.__check_expired_any(qinfo, qname, qclass, now)
        is_cname_qry = qtype == RRType.CNAME()
        expired = False
        ret_resp_list = []
        for trace in [qinfo] + qinfo.cname_trace:
            res_ctx = ResolverContext(qname, qclass, qtype, self.__cache, now,
                                      self.__dbg_level)
            rrset, res_updated, resp_list = res_ctx.resolve()
            ret_resp_list.extend(resp_list)
            if res_updated:
                expired = True
            if (rrset is not None and not is_cname_qry and
                rrset.get_type() == RRType.CNAME()):
                qname = Name(rrset.get_rdata()[0].to_text())
        return expired, ret_resp_list

    def __check_expired_any(self, qinfo, qname, qclass, now):
        rcode, val = self.__cache.find_all(qname, qclass, self.CACHE_OPTIONS)
        rrsets = self.__get_type_any_info(rcode, val)[2]
        if rcode != Rcode.NOERROR():
            res_ctx = ResolverContext(qname, qclass, RRType.ANY(),
                                      self.__cache, now, self.__dbg_level)
            _, res_updated, resp_list = res_ctx.resolve()
            return res_updated, resp_list
        for rrset in rrsets:
            res_ctx = ResolverContext(rrset.get_name(), qclass,
                                      rrset.get_type(), self.__cache, now,
                                      self.__dbg_level)
            _, res_updated, resp_list = res_ctx.resolve()
            if res_updated:
                # If one of the RRsets has expired and been updated, we would
                # have updated all of the "ANY" result.  So don't bother to
                # to replay for the rest; just update the entire cache and
                # we are done.
                qinfo.update(self.__cache, now)
                return True, resp_list
        return False, []

    def __calc_resp_size(self, qry_name, rrsets):
        self.__renderer.clear()
        self.__resp_msg.clear(Message.RENDER)
        # Most of the header fields don't matter for the calculation
        self.__resp_msg.set_opcode(Opcode.QUERY())
        self.__resp_msg.set_rcode(Rcode.NOERROR())
        # Likewise, rrclass and type don't affect the result.
        self.__resp_msg.add_question(Question(qry_name, RRClass.IN(),
                                              RRType.AAAA()))
        for rrset in rrsets:
            # For now, don't bother to try to include SOA for negative resp.
            if rrset.get_rdata_count() == 0:
                continue
            self.__resp_msg.add_rrset(Message.SECTION_ANSWER, rrset)
        self.__resp_msg.to_wire(self.__renderer)
        return len(self.__renderer.get_data())

    def __get_query_trace(self, qry_name, qry_class, qry_type):
        '''Create a new trace object for a query.'''
        # For type ANY queries there's no need for tracing CNAME chain
        if qry_type == RRType.ANY():
            rcode, val = self.__cache.find_all(qry_name, qry_class,
                                               self.CACHE_OPTIONS)
            ttl, ids, rrsets = self.__get_type_any_info(rcode, val)
            return QueryTrace(ttl, ids, rcode), rrsets

        rcode, rrset, id = self.__cache.find(qry_name, qry_class, qry_type,
                                             self.CACHE_OPTIONS)
        # Same for type CNAME query or when it's not CNAME substitution.
        qtrace = QueryTrace(rrset.get_ttl().get_value(), [id], rcode)
        if qry_type == RRType.CNAME() or rrset.get_type() != RRType.CNAME():
            return qtrace, [rrset]

        # This query is subject to CNAME chain.  It's possible it consists
        # a loop, so we need to detect loops and exits.
        rrtype = rrset.get_type()
        if rrtype != RRType.CNAME():
            raise QueryReplaceError('unexpected: type should be CNAME, not ' +
                                    str(rrtype))
        chain_trace = []
        cnames = []
        resp_rrsets = [rrset]
        while rrtype == RRType.CNAME():
            if len(chain_trace) == 16: # safety net: don't try too hard
                break
            cname = Name(rrset.get_rdata()[0].to_text())
            for prev in cnames:
                if cname == prev: # explicit loop detected
                    break
            rcode, rrset, id = self.__cache.find(cname, qry_class, qry_type,
                                                 self.CACHE_OPTIONS)
            chain_trace.append(QueryTrace(rrset.get_ttl().get_value(), [id],
                                          rcode))
            cnames.append(cname)
            resp_rrsets.append(rrset)
            rrtype = rrset.get_type()
        qtrace.cname_trace = chain_trace
        # We return the RCODE for the end of the chain
        qtrace.rcode = rcode
        return qtrace, resp_rrsets

    def __get_type_any_info(self, rcode, find_val):
        ttl = 0
        cache_entries = []
        resp_rrsets = []
        for entry_info in find_val:
            rrset = entry_info[0]
            cache_entries.append(entry_info[1])
            rrset_ttl = rrset.get_ttl().get_value()
            if ttl < rrset_ttl:
                ttl = rrset_ttl
            resp_rrsets.append(rrset)
        return ttl, cache_entries, resp_rrsets

    def __get_query_params(self):
        if self.__query_params is None:
            self.__query_params = list(self.__queries.keys())
            self.__query_params.sort(
                key=lambda x: -self.__queries[x].get_query_count())
        return self.__query_params

    def __update_extqry_stat(self, qinfo, resp_list):
        n_extqry = len(resp_list)
        stats = [self.__extqry_total_stat]
        if qinfo.get_cache_misses() == 1:
            # This is the first creation of this cache entry
            stats.append(self.__extqry1_stat)
        else:
            stats.append(self.__extqry_update_stat)

        for stat in stats:
            if n_extqry not in stat:
                stat[n_extqry] = 0
            stat[n_extqry] += 1

    def __update_extresp_stat(self, resp_list):
        for resp in resp_list:
            self.__extresp_stat[resp[1]] += 1

    def dump_popularity_stat(self, dump_file):
        cumulative_n_qry = 0
        cumulative_cache_hits = 0
        position = 1
        with open(dump_file, 'w') as f:
            f.write(('position,% in total,hit rate,#CNAME,av ext qry,' +
                     'resp-size\n'))
            for qry_param in self.__get_query_params():
                qinfo = self.__queries[qry_param]
                n_queries = qinfo.get_query_count()
                cumulative_n_qry += n_queries
                cumulative_percentage = \
                    (float(cumulative_n_qry) / self.__total_queries) * 100

                cumulative_cache_hits += qinfo.get_cache_hits()
                cumulative_hit_rate = \
                    (float(cumulative_cache_hits) / cumulative_n_qry) * 100

                n_ext_queries_list = qinfo.get_external_query_count()
                n_ext_queries = 0
                for n in n_ext_queries_list:
                    n_ext_queries += n

                f.write('%d,%.2f,%.2f,%d,%.2f,%d\n' %
                        (position, cumulative_percentage, cumulative_hit_rate,
                         len(qinfo.cname_trace),
                         float(n_ext_queries) / len(n_ext_queries_list),
                         qinfo.resp_size))
                position += 1

    def dump_queries(self, dump_file):
        with open(dump_file, 'w') as f:
            for qry_param in self.__get_query_params():
                qinfo = self.__queries[qry_param]
                f.write('%d/%s/%s/%s\n' % (qinfo.get_query_count(),
                                           qry_param[2], qry_param[0],
                                           qry_param[1]))

    def dump_rcode_stat(self):
        print('\nPer RCODE statistics:')
        rcodes = list(self.__rcode_stat.keys())
        rcodes.sort(key=lambda x: -self.__rcode_stat[x])
        for rcode_val in rcodes:
            print('%s: %d' % (Rcode(rcode_val), self.__rcode_stat[rcode_val]))

    def dump_qtype_stat(self):
        print('\nPer Query Type statistics:')
        qtypes = list(self.__qtype_stat.keys())
        qtypes.sort(key=lambda x: -self.__qtype_stat[x])
        for qtype in qtypes:
            print('%s: %d' % (qtype, self.__qtype_stat[qtype]))

    def dump_extqry_stat(self, dump_file):
        stats = [('All', self.__extqry_total_stat),
                 ('Initial Creation', self.__extqry1_stat),
                 ('Update', self.__extqry_update_stat)]
        with open(dump_file, 'w') as f:
            for stat in stats:
                total_qry_count = 0
                total_res_count = 0
                for res_count, qry_count in stat[1].items():
                    total_res_count += res_count * qry_count
                    total_qry_count += qry_count
                av_count = -1
                if total_qry_count > 0: # workaround to avoid div by 0
                    av_count = float(total_res_count) / total_qry_count
                f.write('%s,average=%.2f,count=%d\n' %
                        (stat[0], av_count, total_res_count))
                # dump histogram
                count_list = list(stat[1].keys())
                count_list.sort()
                for count in count_list:
                    f.write('%d,%d\n' % (count, stat[1][count]))

    def dump_extresp_stat(self):
        print('Response statistics:\n')
        descriptions = list(SimpleDNSCache.RESP_DESCRIPTION.keys())
        descriptions.sort()
        print('  %s: %d' % ('Unknown', self.__extresp_stat[0]))
        for type in descriptions:
            print('  %s: %d' % (SimpleDNSCache.RESP_DESCRIPTION[type],
                                self.__extresp_stat[type]))

    def dump_ttl_stat(self, dump_file):
        print('TTL statistics:\n')
        with open(dump_file, 'w') as f:
            self.__cache.dump_ttl_stat(f)

def main(log_file, options):
    cache = SimpleDNSCache()
    cache.load(options.cache_dbfile)
    replay = QueryReplay(log_file, cache, options.dbg_level)
    total_queries, uniq_queries = replay.replay()
    print('Replayed %d queries (%d unique)' % (total_queries, uniq_queries))
    print('%d cache hits (%.2f%%), %d at same TTL' %
          (replay.cache_total_hits,
           (float(replay.cache_total_hits) / total_queries) * 100,
           replay.cache_samettl_hits))
    if options.popularity_file is not None:
        replay.dump_popularity_stat(options.popularity_file)
    if options.query_dump_file is not None:
        replay.dump_queries(options.query_dump_file)
    if options.dump_rcode_stat:
        replay.dump_rcode_stat()
    if options.dump_qtype_stat:
        replay.dump_qtype_stat()
    if options.dump_extqry_file is not None:
        replay.dump_extqry_stat(options.dump_extqry_file)
    if options.dump_resp_stat:
        replay.dump_extresp_stat()
    if options.ttl_stat_file is not None:
        replay.dump_ttl_stat(options.ttl_stat_file)

def get_option_parser():
    parser = OptionParser(usage='usage: %prog [options] log_file')
    parser.add_option("-c", "--cache-dbfile",
                      dest="cache_dbfile", action="store", default=None,
                      help="Serialized DNS cache DB")
    parser.add_option("-d", "--dbg-level", dest="dbg_level", action="store",
                      default=0,
                      help="specify the verbosity level of debug output")
    parser.add_option("-i", "--interactive", dest="interactive",
                      action="store_true", default=False,
                      help="enter interactive cache session on finding a bug")
    parser.add_option("-o", "--overrride",
                      dest="override", action="store_true",
                      default=False,
                      help="run 'override' mode, purging lower rank caches")
    parser.add_option("-p", "--dump-popularity",
                      dest="popularity_file", action="store",
                      help="file to dump statistics per query popularity")
    parser.add_option("-q", "--dump-queries",
                      dest="query_dump_file", action="store",
                      help="dump unique queries")
    parser.add_option("-Q", "--extqry-stat-file",
                      dest="dump_extqry_file", action="store",
                      help="file to dump statistics on external queries")
    parser.add_option("-r", "--dump-rcode-stat",
                      dest="dump_rcode_stat", action="store_true",
                      default=False, help="dump per RCODE statistics")
    parser.add_option("-R", "--dump-response-stat",
                      dest="dump_resp_stat", action="store_true",
                      default=False,
                      help="dump statistics about external responses")
    parser.add_option("-t", "--dump-qtype-stat",
                      dest="dump_qtype_stat", action="store_true",
                      default=False, help="dump per type statistics")
    parser.add_option("-T", "--ttl-stat-file",
                      dest="ttl_stat_file", action="store",
                      help="dump cache TTL statistics to the file")
    return parser

if __name__ == "__main__":
    parser = get_option_parser()
    (options, args) = parser.parse_args()

    if len(args) == 0:
        parser.error('input file is missing')
    if options.cache_dbfile is None:
        parser.error('cache DB file is mandatory')
    main(args[0], options)
