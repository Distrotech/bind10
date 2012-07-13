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
import dns_cache

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

    def add_cache_log(self, cache_log):
        self.__cache_log.append(cache_log)

    def get_last_cache(self):
        if len(self.__cache_log) == 0:
            return None
        return self.__cache_log[-1]

    def cache_expired(self, cache, now):
        '''Check if the cache for this query has expired or is still valid.

        For type ANY query, __cache_entries may contain multiple entry IDs.
        In that case we consider it valid as long as one of them is valid.

        '''
        expired = 0
        for cache_entry_id in self.__cache_entries:
            entry = cache.get(cache_entry_id)
            if entry.is_expired(now):
                expired += 1
        return len(self.__cache_entries) == expired

class CacheLog:
    '''consists of...

    __time_created (float): Timestamp when an answer for the query is cached.
    hits (int): number of cache hits
    misses (int): 1 if this is created on cache miss (normal); otherwise 0
    TBD:
      number of external queries involved along with the response sizes

    '''
    def __init__(self, now, on_miss=True):
        self.__time_created = now
        self.hits = 0
        self.misses = 1 if on_miss else 0

class ResolverContext:
    '''Emulated resolver context.'''
    def __init__(self, qname, qclass, qtype, cache, now, dbg_level):
        self.__qname = qname
        self.__qclass = qclass
        self.__qtype = qtype
        self.__cache = cache
        self.__now = now
        self.__dbg_level = dbg_level
        self.__cur_zone = qname # sentinel

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

            have_addr, fetch_list = self.__find_ns_addrs(nameservers)
            if not have_addr:
                # If fetching NS addresses fail, we should be at the end of
                # chain
                found, fetch_resps = self.__fetch_ns_addrs(fetch_list)
                resp_list.extend(fetch_resps)
                if not found:
                    chain = chain[:-1]
                    if len(chain) != 1:
                        raise QueryReplaceError('assumption failure')

            # "send query, then get response".  Now we can consider the
            # delegation records (NS and glue AAAA/A) one level deepr active
            # again.
            self.dprint(LOGLVL_DEBUG10, 'external query')
            chain = chain[:-1]
            if len(chain) == 1: # reached deepest zone, update the final cache
                break

            # Otherwise, go down to the zone one level lower.
            new_id, nameservers = chain[-1]
            self.dprint(LOGLVL_DEBUG10, 'update NS at zone %s, trust %s',
                        [nameservers.get_name(),
                         self.__cache.get(new_id).trust])
            self.__cache.update(new_id, self.__now)
            self.__update_glue(nameservers)

        # We've reached the deepest zone (which should normally contain the
        # query name)
        self.dprint(LOGLVL_DEBUG5, 'resolution completed')
        self.__cache.update(chain[0][0], self.__now)
        return chain[0][1], True, resp_list

    def __fetch_ns_addrs(self, fetch_list):
        self.dprint(LOGLVL_DEBUG10, 'no NS addresses are known, fetch them.')
        ret_resp_list = []
        for fetch in fetch_list:
            res_ctx = ResolverContext(fetch[1], self.__qclass, fetch[0],
                                      self.__cache, self.__now,
                                      self.__dbg_level)
            rrset, updated, resp_list = res_ctx.resolve()
            ret_resp_list.extend(resp_list)
            if not updated:
                raise QueryReplaceError('assumption failure')
            if rrset.get_rdata_count() > 0: # positive result
                self.dprint(LOGLVL_DEBUG10, 'fetching an NS address succeeded')
                return True, ret_resp_list
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
                            'Exclude out-ofzone glue: %s', [ns_name])
                continue
            _, rrset, id = self.__find_delegate_info(ns_name, RRType.A())
            if id is not None:
                self.dprint(LOGLVL_DEBUG10, 'Update IPv4 glue: %s', [ns_name])
                self.__cache.update(id, self.__now)
            _, rrset, id = self.__find_delegate_info(ns_name, RRType.AAAA())
            if id is not None:
                self.dprint(LOGLVL_DEBUG10, 'Update IPv6 glue: %s', [ns_name])
                self.__cache.update(id, self.__now)

    def __get_resolve_chain(self):
        chain = []
        resp_list = []
        rcode, answer_rrset, id = \
            self.__cache.find(self.__qname, self.__qclass, self.__qtype,
                              dns_cache.SimpleDNSCache.FIND_ALLOW_CNAME |
                              dns_cache.SimpleDNSCache.FIND_ALLOW_NEGATIVE)
        entry = self.__cache.get(id)
        chain.append((id, answer_rrset))
        resp_list.append(entry.msglen)
        if not entry.is_expired(self.__now):
            return chain, []

        for l in range(0, self.__qname.get_labelcount()):
            zname = self.__qname.split(l)
            _, ns_rrset, id = self.__find_delegate_info(zname, RRType.NS())
            if ns_rrset is None:
                continue
            entry = self.__cache.get(id)
            self.dprint(LOGLVL_DEBUG10, 'build resolve chain at %s, trust %s',
                        [zname, entry.trust])
            chain.append((id, ns_rrset))
            if not entry.is_expired(self.__now):
                return chain, resp_list
            resp_list.append(entry.msglen)

        # In our setup root server should be always available.
        raise QueryReplaceError('no name server found for ' + str(qname))

    def __find_ns_addrs(self, nameservers):
        # We only need to know whether we have at least one usable address:
        have_address = False

        # Record any missing address to be fetched.
        fetch_list = []
        for ns_name in [Name(ns.to_text()) for ns in nameservers.get_rdata()]:
            rcode, rrset4, id = self.__find_delegate_info(ns_name, RRType.A(),
                                                          True)
            if rrset4 is not None:
                self.dprint(LOGLVL_DEBUG10, 'found IPv4 address for NS %s',
                            [ns_name])
                have_address = True
            elif rcode is None:
                fetch_list.append((RRType.A(), ns_name))

            rcode, rrset6, id = \
                self.__find_delegate_info(ns_name, RRType.AAAA(), True)
            if rrset6 is not None:
                self.dprint(LOGLVL_DEBUG10, 'found IPv6 address for NS %s',
                            [ns_name])
                have_address = True
            elif rcode is None:
                fetch_list.append((RRType.AAAA(), ns_name))

        return have_address, fetch_list

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
                              dns_cache.SimpleDNSCache.FIND_ALLOW_NEGATIVE)
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
                                  dns_cache.SimpleDNSCache.FIND_ALLOW_NOANSWER,
                                  dns_cache.SimpleDNSCache.TRUST_AUTHAUTHORITY)
            if (rrset is not None and
                not self.__cache.get(id).is_expired(self.__now)):
                return rcode, rrset, id

        # Finally try delegation or glues.  checking rcode is meaningless
        # like the previous case.
        # Unlike other cases, we use it whether it's active or expired unless
        # explicitly requested to exclude expired ones.
        rcode, rrset, id = \
            self.__cache.find(name, self.__qclass, rrtype,
                              dns_cache.SimpleDNSCache.FIND_ALLOW_NOANSWER,
                              dns_cache.SimpleDNSCache.TRUST_GLUE)
        if (rrset is not None and
            (not active_only or
             not self.__cache.get(id).is_expired(self.__now))):
            return rcode, rrset, id

        return None, None, None

class QueryReplay:
    CACHE_OPTIONS = dns_cache.SimpleDNSCache.FIND_ALLOW_CNAME | \
        dns_cache.SimpleDNSCache.FIND_ALLOW_NEGATIVE

    def __init__(self, log_file, cache, dbg_level):
        self.__log_file = log_file
        self.__cache = cache
        # Replay result.
        # dict from (Name, RR type, RR class) to QueryTrace
        self.__queries = {}
        self.__total_queries = 0
        self.__query_params = None
        self.__cur_query = None # use for debug out
        self.__dbg_level = int(dbg_level)
        self.__resp_msg = Message(Message.RENDER) # for resp size estimation
        self.__renderer = MessageRenderer()
        self.__rcode_stat = {}  # RCODE value (int) => query counter
        self.__qtype_stat = {} # RR type => query counter

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
            self.dprint(LOGLVL_DEBUG3, 'cache miss, updated with %s messages',
                        [len(resp_list), resp_list])
            cache_log = CacheLog(qry_time)
            qinfo.add_cache_log(cache_log)
        else:
            self.dprint(LOGLVL_DEBUG3, 'cache hit')
            cache_log = qinfo.get_last_cache()
            if cache_log is None:
                cache_log = CacheLog(qry_time, False)
                qinfo.add_cache_log(cache_log)
            cache_log.hits += 1

    def __check_expired(self, qinfo, qname, qclass, qtype, now):
        if qtype == RRType.ANY():
            raise QueryReplaceError('ANY query is not supported yet')
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

    def dump_popularity_stat(self, dump_file):
        cumulative_n_qry = 0
        cumulative_cache_hits = 0
        position = 1
        with open(dump_file, 'w') as f:
            f.write('position,% in total,hit rate,#CNAME,resp-size\n')
            for qry_param in self.__get_query_params():
                qinfo = self.__queries[qry_param]
                n_queries = qinfo.get_query_count()
                cumulative_n_qry += n_queries
                cumulative_percentage = \
                    (float(cumulative_n_qry) / self.__total_queries) * 100

                cumulative_cache_hits += qinfo.get_cache_hits()
                cumulative_hit_rate = \
                    (float(cumulative_cache_hits) / cumulative_n_qry) * 100

                f.write('%d,%.2f,%.2f,%d,%d\n' %
                        (position, cumulative_percentage, cumulative_hit_rate,
                         len(qinfo.cname_trace), qinfo.resp_size))
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

def main(log_file, options):
    cache = dns_cache.SimpleDNSCache()
    cache.load(options.cache_dbfile)
    replay = QueryReplay(log_file, cache, options.dbg_level)
    total_queries, uniq_queries = replay.replay()
    print('Replayed %d queries (%d unique)' % (total_queries, uniq_queries))
    if options.popularity_file is not None:
        replay.dump_popularity_stat(options.popularity_file)
    if options.query_dump_file is not None:
        replay.dump_queries(options.query_dump_file)
    if options.dump_rcode_stat:
        replay.dump_rcode_stat()
    if options.dump_qtype_stat:
        replay.dump_qtype_stat()

def get_option_parser():
    parser = OptionParser(usage='usage: %prog [options] log_file')
    parser.add_option("-c", "--cache-dbfile",
                      dest="cache_dbfile", action="store", default=None,
                      help="Serialized DNS cache DB")
    parser.add_option("-d", "--dbg-level", dest="dbg_level", action="store",
                      default=0,
                      help="specify the verbosity level of debug output")
    parser.add_option("-p", "--dump-popularity",
                      dest="popularity_file", action="store",
                      help="dump statistics per query popularity")
    parser.add_option("-q", "--dump-queries",
                      dest="query_dump_file", action="store",
                      help="dump unique queries")
    parser.add_option("-r", "--dump-rcode-stat",
                      dest="dump_rcode_stat", action="store_true",
                      default=False, help="dump per RCODE statistics")
    parser.add_option("-t", "--dump-qtype-stat",
                      dest="dump_qtype_stat", action="store_true",
                      default=False, help="dump per type statistics")
    return parser

if __name__ == "__main__":
    parser = get_option_parser()
    (options, args) = parser.parse_args()

    if len(args) == 0:
        parser.error('input file is missing')
    if options.cache_dbfile is None:
        parser.error('cache DB file is mandatory')
    main(args[0], options)
