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
from optparse import OptionParser
import struct

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
    '''Cache entry stored in SimpleDNSCache.

    This is essentially a straightforward tuple, just giving an intuitive name
    to each entry.  The attributes are:
    ttl (int) The TTL of the cache entry.
    rdata_list (list of isc.dns.Rdatas) The list of RDATAs for the entry.
      Can be an empty list for negative cache entries.
    trust (SimpleDNSCache.TRUST_xxx) The trust level of the cache entry.
    msglen (int) The size of the DNS response message from which the cache
      entry comes; it's 0 if it's not a result of a DNS message.
    rcode (int) Numeric form of corresponding RCODE (converted to int as it's
      more memory efficient).

    '''

    def __init__(self, ttl, rdata_list, trust, msglen, rcode, id):
        self.ttl = ttl
        self.rdata_list = rdata_list
        self.trust = trust
        self.msglen = msglen
        self.rcode = rcode.get_code()
        self.id = id
        self.time_updated = None # timestamp of 'creation' or 'update'

    def copy(self, other):
        self.ttl = other.ttl
        self.rdata_list = other.rdata_list
        self.trust = other.trust
        self.msglen = other.msglen
        self.rcode = other.rcode
        self.id = other.id

    def is_expired(self, now):
        # This is cheating, but for now we assume all "local" entries have
        # permanent TTL
        if self.trust == SimpleDNSCache.TRUST_LOCAL:
            return False

        if self.time_updated is None or self.time_updated + self.ttl < now:
            return True
        return False

# Don't worry about cache expire; just record the RRs
class SimpleDNSCache:
    '''A simplified DNS cache database.

    It's a dict from (isc.dns.Name, isc.dns.RRClass) to an entry.
    Each entry can be of either of the following:
    - CacheEntry: in case the specified name doesn't exist (NXDOMAIN).
    - dict from RRType to list of CacheEntry: this gives a cache entries for
       the (name, class, type) sorted by the trust levels (more trustworthy
       ones appear sooner)

    '''

    # simplified trust levels for cached records
    TRUST_LOCAL = 0             # specific this implementation, never expires
    TRUST_ANSWER = 1            # authoritative answer
    TRUST_AUTHAUTHORITY = 2     # authority section records in auth answer
    TRUST_GLUE = 3              # referral or glue
    TRUST_AUTHADDITIONAL = 4    # additional section records in auth answer

    # Search options, can be logically OR'ed.
    FIND_DEFAULT = 0
    FIND_ALLOW_NEGATIVE = 1
    FIND_ALLOW_NOANSWER = 2
    FIND_ALLOW_CNAME = 4

    # Response types (when applicable)
    RESP_FINAL_ANSWER_COMPRESSED = 0
    RESP_FINAL_ANSWER_UNCOMPRESSED = 1
    RESP_CNAME_ANSWER_COMPRESSED = 2
    RESP_CNAME_ANSWER_UNCOMPRESSED = 3
    RESP_ANSWER_UNEXPECTED = 4
    RESP_NXDOMAIN_SOA = 5
    RESP_NXDOMAIN_NOAUTH = 6
    RESP_NXDOMAIN_UNEXPECTED = 7
    RESP_NXRRSET_SOA = 8
    RESP_NXRRSET_NOAUTH = 9
    RESP_NXRRSET_UNEXPECTED = 10
    RESP_REFERRAL_WITHGLUE = 11
    RESP_REFERRAL_WITHOUTGLUE = 12
    RESP_REFERRAL_UNEXPECTED = 13
    RESP_UNEXPECTED = 14

    def __init__(self):
        # top level cache table
        self.__table = {}
        self.__counter = 0      # unique ID for CacheEntry's
        self.__entries = {}     # ID => CacheEntry

    def get(self, entry_id):
        return self.__entries[entry_id]

    def find(self, name, rrclass, rrtype, options=FIND_DEFAULT, trust=None):
        '''Find a specified (name, class, type) from the cache.

        options are a combination of various search options.
        if trust is specified, only the specified trust level is considered.
        If this is specified , FIND_ALLOW_NOANSWER option shouldn't be set.

        '''
        key = (name, rrclass)
        rdata_map = self.__table.get((name, rrclass))
        rcode, rrset, trust, id = self.__find_type(name, rrclass, rrtype,
                                                   rdata_map, options, trust)
        if ((options & self.FIND_ALLOW_CNAME) != 0 and
            rrtype != RRType.CNAME()):
            # If CNAME is allowed, see if there is one.  If there is and it
            # has a higher trust level, we use it.
            rcode_cname, rrset_cname, trust_cname, id_cname = \
                self.__find_type(name, rrclass, RRType.CNAME(), rdata_map,
                                 options, trust)
            if (rrset_cname is not None and
                (trust is None or trust_cname < trust)):
                return rcode_cname, rrset_cname, id_cname
        return rcode, rrset, id

    def __find_nxdomain_dummy(self):
        if key in self.__table and isinstance(self.__table[key], CacheEntry):
            # the name doesn't exist.
            # lazy shortcut: we assume NXDOMAIN is always authoritative,
            # skipping trust level check
            rcode = Rcode(self.__table[key].rcode)
            if (options & self.FIND_ALLOW_NEGATIVE) != 0:
                return rcode, RRset(name, rrclass, rrtype,
                                    RRTTL(self.__table[key].ttl))
            else:
                return rcode, None

    def __find_type(self, name, rrclass, rrtype, rdata_map, options, trust):
        '''A subroutine of find, finding an RRset of a given type.'''
        if rdata_map is not None and rrtype in rdata_map:
            entries = rdata_map[rrtype]
            entry = entries[0]
            if (options & self.FIND_ALLOW_NOANSWER) == 0:
                entry = self.__find_cache_entry(entries, self.TRUST_ANSWER)
            elif trust is not None:
                entry = self.__find_cache_entry(entries, trust, True)
            if entry is None:
                return None, None, None, None
            rcode = Rcode(entry.rcode)
            (ttl, rdata_list) = (entry.ttl, entry.rdata_list)
            rrset = RRset(name, rrclass, rrtype, RRTTL(ttl))
            for rdata in rdata_list:
                rrset.add_rdata(rdata)
            if rrset.get_rdata_count() == 0 and \
                    (options & self.FIND_ALLOW_NEGATIVE) == 0:
                return rcode, None, None, None
            return rcode, rrset, entry.trust, entry.id
        return None, None, None, None

        if key in self.__table and isinstance(self.__table[key], CacheEntry):
            rcode = Rcode(self.__table[key].rcode)
            if (options & self.FIND_ALLOW_NEGATIVE) != 0:
                return rcode, RRset(name, rrclass, RRType.ANY(),
                                    RRTTL(self.__table[key].ttl))
            else:
                return rcode, None

    def find_all(self, name, rrclass, options=FIND_DEFAULT):
        rdata_map = self.__table.get((name, rrclass))
        rrsets = []
        for rrtype, entries in rdata_map.items():
            entry = entries[0]
            if (options & self.FIND_ALLOW_NOANSWER) == 0:
                entry = self.__find_cache_entry(entries, self.TRUST_ANSWER)
            if entry is None:
                continue
            rcode = Rcode(entry.rcode)

            # If there's at least one NXDOMAIN result, this cancels all others.
            if rcode == Rcode.NXDOMAIN():
                nx_rrset = RRset(name, rrclass, RRType.ANY(),
                                 RRTTL(entry.ttl))
                return rcode, [(nx_rrset, entry.id)]

            (ttl, rdata_list) = (entry.ttl, entry.rdata_list)
            if len(rdata_list) == 0: # skip empty type
                continue
            rrset = RRset(name, rrclass, rrtype, RRTTL(ttl))
            for rdata in rdata_list:
                rrset.add_rdata(rdata)
            rrsets.append((rrset, entry.id))
        rcode = Rcode.NOERROR() if len(rrsets) > 0 else Rcode.NXRRSET()
        return rcode, rrsets

    def update(self, entry_id, now):
        '''Check the specified cache entry is still "alive" wrt TTL.

        If not, update its update/creation time to "now".
        Return True if the timestamp is updated; False otherwise.

        '''
        entry = self.__entries[entry_id]
        if entry.is_expired(now):
            entry.time_updated = now
            return True
        return False

    def add(self, rrset, trust=TRUST_LOCAL, msglen=0, rcode=Rcode.NOERROR()):
        '''Add a new cache item.

        Note: this cache always handles cached data per RR type; even if
        NXDOMAIN is type independent, it's still specific to the associated
        type within this cache.

        '''
        key = (rrset.get_name(), rrset.get_class())
        new_entry = self.__create_cache_entry(rrset.get_ttl().get_value(),
                                              rrset.get_rdata(), trust, msglen,
                                              rcode)
        if not key in self.__table:
            self.__table[key] = {rrset.get_type(): [new_entry]}
            new_entry._table_entry = self.__table[key]
        else:
            table_ent = self.__table[key]
            cur_entries = table_ent.get(rrset.get_type())
            if cur_entries is None:
                table_ent[rrset.get_type()] = [new_entry]
            else:
                self.__insert_cache_entry(cur_entries, new_entry)

    def __create_cache_entry(self, ttl, rdata_list, trust, msglen, rcode):
        new_entry = CacheEntry(ttl, rdata_list, trust, msglen, rcode,
                               self.__counter)
        self.__entries[self.__counter] = new_entry
        self.__counter += 1
        return new_entry

    def __insert_cache_entry(self, entries, new_entry):
        old = self.__find_cache_entry(entries, new_entry.trust, True)
        if old is not None and old.trust == new_entry.trust:
            old.copy(new_entry)
        else:
            entries.append(new_entry)
            entries.sort(key=lambda x: x.trust)

    def __find_cache_entry(self, entries, trust, exact=False):
        for entry in entries:
            if entry.trust == trust or (not exact and entry.trust < trust):
                return entry
        return None

    def dump(self, dump_file, serialize=False):
        if serialize:
            with open(dump_file, 'bw') as f:
                self.__serialize(f)
        else:
            with open(dump_file, 'w') as f:
                self.__dump_text(f)

    def load(self, db_file):
        with open(db_file, 'br') as f:
            self.__deserialize(f)

    def __dump_text(self, f):
        for key, entry in self.__table.items():
            name = key[0]
            rrclass = key[1]
            rdata_map = entry
            for rrtype, entries in rdata_map.items():
                for entry in entries:
                    if len(entry.rdata_list) == 0:
                        f.write(';; [%s, TTL=%d, msglen=%d] %s/%s/%s\n' %
                                (str(Rcode(entry.rcode)), entry.ttl,
                                 entry.msglen, str(name), str(rrclass),
                                 str(rrtype)))
                    else:
                        f.write(';; [msglen=%d, trust=%d]\n' %
                                (entry.msglen, entry.trust))
                        rrset = RRset(name, rrclass, rrtype, RRTTL(entry.ttl))
                        for rdata in entry.rdata_list:
                            rrset.add_rdata(rdata)
                        f.write(rrset.to_text())

    def __serialize(self, f):
        '''Dump cache database content to a file in serialized binary format.

        The serialized format is as follows:
        Common header part:
          <name length, 1 byte>
          <domain name (wire)>
          <RR class (numeric, wire)>
          <# of cache entries, 2 bytes>
          Sequence of serialized cache entries.  Each of which is:
            <RR type value, wire>
            <# of cache entries of the type, 1 byte>
            Sequence of cache entries of the type, each of which is:
              <RCODE value, 1 byte>
              <TTL, 4 bytes>
              <msglen, 2 bytes>
              <trust, 1 byte>
              <# of RDATAs, 2 bytes>
              sequence of RDATA, each of which is:
                <RDATA length, 2 bytes>
                <RDATA, wire>

        '''
        for key, entry in self.__table.items():
            name = key[0]
            rrclass = key[1]
            f.write(struct.pack('B', name.get_length()))
            f.write(name.to_wire(b''))
            f.write(rrclass.to_wire(b''))

            rdata_map = entry
            data = struct.pack('H', len(rdata_map)) # #-of-cache entries
            for rrtype, entries in rdata_map.items():
                data += rrtype.to_wire(b'')
                data += struct.pack('B', len(entries))

                for entry in entries:
                    data += struct.pack('B', entry.rcode)
                    data += struct.pack('I', entry.ttl)
                    data += struct.pack('H', entry.msglen)
                    data += struct.pack('B', entry.trust)
                    data += struct.pack('H', len(entry.rdata_list))
                    for rdata in entry.rdata_list:
                        rdata_data = rdata.to_wire(b'')
                        data += struct.pack('H', len(rdata_data))
                        data += rdata_data
            f.write(data)

    def __deserialize(self, f):
        '''Load serialized cache DB to memory.

        See __serialize for the format.  Validation is generally omitted
        for simplicity.

        '''
        while True:
            initial_byte = f.read(1)
            if len(initial_byte) == 0:
                break
            ndata = f.read(struct.unpack('B', initial_byte)[0])
            name = Name(ndata)
            rrclass = RRClass(f.read(2))
            key = (name, rrclass)
            n_types = struct.unpack('H', f.read(2))[0]

            self.__table[key] = {}
            while n_types > 0:
                n_types -= 1
                rrtype = RRType(f.read(2))
                n_entries = struct.unpack('B', f.read(1))[0]
                entries = []
                while n_entries > 0:
                    n_entries -= 1
                    rcode = struct.unpack('B', f.read(1))[0]
                    ttl = struct.unpack('I', f.read(4))[0]
                    msglen = struct.unpack('H', f.read(2))[0]
                    trust = struct.unpack('B', f.read(1))[0]
                    n_rdata = struct.unpack('H', f.read(2))[0]
                    rdata_list = []
                    while n_rdata > 0:
                        n_rdata -= 1
                        rdata_len = struct.unpack('H', f.read(2))[0]
                        rdata_list.append(Rdata(rrtype, rrclass,
                                                f.read(rdata_len)))
                    entry = self.__create_cache_entry(ttl, rdata_list, trust,
                                                      msglen, Rcode(rcode))
                    entries.append(entry)
                entries.sort(key=lambda x: x.trust)
                self.__table[key][rrtype] = entries

def get_option_parser():
    parser = OptionParser(usage='usage: %prog [options] cache_db_file')
    parser.add_option("-f", "--dump-file", dest="dump_file", action="store",
                      default=None,
                      help="if specified, file name to dump the cache " + \
                          "content in text format")
    return parser

def run(db_file, options):
    cache = SimpleDNSCache()
    cache.load(db_file)
    if options.dump_file is not None:
        cache.dump(options.dump_file)

if __name__ == '__main__':
    parser = get_option_parser()
    (options, args) = parser.parse_args()
    if len(args) == 0:
        parser.error('cache DB file is missing')
    run(args[0], options)
