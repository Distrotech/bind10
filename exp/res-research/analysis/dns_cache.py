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

# "root hint"
ROOT_SERVERS = [pfx + '.root-servers.net' for pfx in 'abcdefghijklm']
ROOT_V4ADDRS = {'a': '198.41.0.4', 'b': '192.228.79.201', 'c': '192.33.4.12',
                'd': '128.8.10.90', 'e': '192.203.230.10', 'f': '192.5.5.241',
                'g': '192.112.36.4', 'h': '128.63.2.53', 'i': '192.36.148.17',
                'j': '192.58.128.30', 'k': '193.0.14.129', 'l': '199.7.83.42',
                'm': '202.12.27.33'}
ROOT_V6ADDRS = {'a': '2001:503:ba3e::2:30', 'd': '2001:500:2d::d',
                'f': '2001:500:2f::f', 'h': '2001:500:1::803f:235',
                'i': '2001:7fe::53', 'k': '2001:7fd::1', 'l': '2001:500:3::42',
                'm': '2001:dc3::35'}

def install_root_hint(cache):
    '''Install the hardcoded "root hint" to the given DNS cache.

    cache is a SimpleDNSCache object.

    '''
    root_ns = RRset(Name("."), RRClass.IN(), RRType.NS(), RRTTL(518400))
    for ns in ROOT_SERVERS:
        root_ns.add_rdata(Rdata(RRType.NS(), RRClass.IN(), ns))
    cache.add(root_ns, SimpleDNSCache.TRUST_LOCAL)
    for pfx in ROOT_V4ADDRS.keys():
        ns_name = Name(pfx + '.root-servers.net')
        rrset = RRset(ns_name, RRClass.IN(), RRType.A(), RRTTL(3600000))
        rrset.add_rdata(Rdata(RRType.A(), RRClass.IN(), ROOT_V4ADDRS[pfx]))
        cache.add(rrset, SimpleDNSCache.TRUST_LOCAL)
    for pfx in ROOT_V6ADDRS.keys():
        ns_name = Name(pfx + '.root-servers.net')
        rrset = RRset(ns_name, RRClass.IN(), RRType.AAAA(), RRTTL(3600000))
        rrset.add_rdata(Rdata(RRType.AAAA(), RRClass.IN(), ROOT_V6ADDRS[pfx]))
        cache.add(rrset, SimpleDNSCache.TRUST_LOCAL)

class CacheEntry:
    '''Cache entry stored in SimpleDNSCache.'''

    def __init__(self, ttl, rdata_list, trust):
        self.ttl = ttl
        self.rdata_list = rdata_list
        self.trust = trust

# Don't worry about cache expire; just record the RRs
class SimpleDNSCache:
    # simplified trust levels for cached records
    TRUST_LOCAL = 0             # specific this implementation, never expires
    TRUST_ANSWER = 1            # authoritative answer
    TRUST_GLUE = 2              # referral or glue

    # Search options, can be logically OR'ed.
    FIND_DEFAULT = 0
    FIND_ALLOW_NEGATIVE = 1
    FIND_ALLOW_GLUE = 2
    FIND_ALLOW_CNAME = 4

    def __init__(self):
        # top level cache table: (name, class) => rdata_map
        # rdata_map: type=>CacheEntry
        # TODO: negative cache
        self.__table = {}

    def find(self, name, rrclass, rrtype, options=FIND_DEFAULT):
        key = (name, rrclass)
        if key in self.__table and isinstance(self.__table[key], RRset):
            # the name doesn't exist; the dict value is its negative TTL.
            # lazy shortcut: we assume NXDOMAIN is always authoritative,
            # skipping trust level check
            if (options & self.FIND_ALLOW_NEGATIVE) != 0:
                return self.__table[key]
            else:
                return None
        rdata_map = self.__table.get((name, rrclass))
        search_types = [rrtype]
        if (options & self.FIND_ALLOW_CNAME) != 0 and \
                rrtype != RRType.CNAME():
            search_types.append(RRType.CNAME())
        for type in search_types:
            if rdata_map is not None and type in rdata_map:
                entry = rdata_map[type]
                if (options & self.FIND_ALLOW_GLUE) == 0 and \
                        entry.trust > self.TRUST_ANSWER:
                    return None
                (ttl, rdata_list) = (entry.ttl, entry.rdata_list)
                rrset = RRset(name, rrclass, type, RRTTL(ttl))
                for rdata in rdata_list:
                    rrset.add_rdata(rdata)
                if rrset.get_rdata_count() == 0 and \
                        (options & self.FIND_ALLOW_NEGATIVE) == 0:
                    return None
                return rrset
        return None

    def add(self, rrset, trust=TRUST_LOCAL, rcode=Rcode.NOERROR()):
        key = (rrset.get_name(), rrset.get_class())
        if rcode == Rcode.NXDOMAIN():
            # Special case for NXDOMAIN: the table consists of a single entry
            # of its TTL
            self.__table[key] = rrset
            return
        elif key in self.__table and isinstance(self.__table[key], RRset):
            # Overriding a previously-NXDOMAIN cache entry
            del self.__table[key]
        new_entry = CacheEntry(rrset.get_ttl().get_value(), rrset.get_rdata(),
                               trust)
        if not key in self.__table:
            self.__table[key] = {rrset.get_type(): new_entry}
        else:
            table_ent = self.__table[key]
            cur_entry = table_ent.get(rrset.get_type())
            if cur_entry is None or cur_entry.trust >= trust:
                table_ent[rrset.get_type()] = new_entry
