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
from optparse import OptionParser
import re
import sys

convert_rrtype = parse_qrylog.convert_rrtype
RE_LOGLINE = parse_qrylog.RE_LOGLINE

class QueryReplaceError(Exception):
    pass

class QueryTrace:
    '''consists of...

    ttl(int): TTL
    resp_size(int): estimated size of the response to the query.

    '''
    def __init__(self, ttl, cache_entries):
        self.ttl = ttl
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
        expired = False
        for trace in [self] + self.cname_trace:
            if trace.__cache_expired(cache, now):
               expired = True
        return expired

    def __cache_expired(self, cache, now):
        updated = 0
        for cache_entry_id in self.__cache_entries:
            if cache.update(cache_entry_id, now):
                updated += 1
        return len(self.__cache_entries) == updated

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

class QueryReplay:
    CACHE_OPTIONS = dns_cache.SimpleDNSCache.FIND_ALLOW_CNAME | \
        dns_cache.SimpleDNSCache.FIND_ALLOW_NEGATIVE

    def __init__(self, log_file, cache):
        self.__log_file = log_file
        self.__cache = cache
        # Replay result.
        # dict from (Name, RR type, RR class) to QueryTrace
        self.__queries = {}
        self.__total_queries = 0
        self.__query_params = None
        self.__resp_msg = Message(Message.RENDER) # for resp size estimation
        self.__renderer = MessageRenderer()

    def replay(self):
        with open(self.__log_file) as f:
            for log_line in f:
                self.__total_queries += 1
                try:
                    self.__replay_query(log_line)
                except Exception as ex:
                    sys.stderr.write('error (' + str(ex) + ') at line: ' +
                                     log_line)
                    raise ex
        return self.__total_queries, len(self.__queries)

    def __replay_query(self, log_line):
        '''Replay a single query.'''
        m = re.match(RE_LOGLINE, log_line)
        if not m:
            sys.stderr.write('unexpected line: ' + log_line)
            return
        qry_time = float(m.group(1))
        client_addr = m.group(2)
        qry_name = Name(m.group(3))
        qry_class = RRClass(m.group(4))
        qry_type = RRType(convert_rrtype(m.group(5)))
        qry_key = (qry_name, qry_type, qry_class)

        qinfo = self.__queries.get(qry_key)
        if qinfo is None:
            qinfo, rrsets = \
                self.__get_query_trace(qry_name, qry_class, qry_type)
            qinfo.resp_size = self.__calc_resp_size(qry_name, rrsets)
            self.__queries[qry_key] = qinfo
        if qinfo.cache_expired(self.__cache, qry_time):
            cache_log = CacheLog(qry_time)
            qinfo.add_cache_log(cache_log)
        else:
            cache_log = qinfo.get_last_cache()
            if cache_log is None:
                cache_log = CacheLog(qry_time, False)
                qinfo.add_cache_log(cache_log)
            cache_log.hits += 1

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
            return QueryTrace(ttl, ids), rrsets

        rcode, rrset, id = self.__cache.find(qry_name, qry_class, qry_type,
                                             self.CACHE_OPTIONS)
        # Same for type CNAME query or when it's not CNAME substitution.
        qtrace = QueryTrace(rrset.get_ttl().get_value(), [id])
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
            try:
                chain_trace.append(QueryTrace(rrset.get_ttl().get_value(),
                                              [id]))
                cnames.append(cname)
                resp_rrsets.append(rrset)
                rrtype = rrset.get_type()
            except Exception as ex:
                sys.stderr.write('CNAME trace failed for %s/%s/%s at %s\n' %
                                 (qry_name, qry_class, qry_type, cname))
                break
        qtrace.cname_trace = chain_trace
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

def main(log_file, options):
    cache = dns_cache.SimpleDNSCache()
    cache.load(options.cache_dbfile)
    replay = QueryReplay(log_file, cache)
    total_queries, uniq_queries = replay.replay()
    print('Replayed %d queries (%d unique)' % (total_queries, uniq_queries))
    if options.popularity_file is not None:
        replay.dump_popularity_stat(options.popularity_file)
    if options.query_dump_file is not None:
        replay.dump_queries(options.query_dump_file)

def get_option_parser():
    parser = OptionParser(usage='usage: %prog [options] log_file')
    parser.add_option("-c", "--cache-dbfile",
                      dest="cache_dbfile", action="store", default=None,
                      help="Serialized DNS cache DB")
    parser.add_option("-p", "--dump-popularity",
                      dest="popularity_file", action="store",
                      help="dump statistics per query popularity")
    parser.add_option("-q", "--dump-queries",
                      dest="query_dump_file", action="store",
                      help="dump unique queries")
    return parser

if __name__ == "__main__":
    parser = get_option_parser()
    (options, args) = parser.parse_args()

    if len(args) == 0:
        parser.error('input file is missing')
    if options.cache_dbfile is None:
        parser.error('cache DB file is mandatory')
    main(args[0], options)
