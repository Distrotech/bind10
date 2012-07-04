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

'''A simple generator of DNS queries and responses.'''

from isc.dns import *
import socket
import random
from optparse import OptionParser

usage = 'usage: %prog [options]'
parser = OptionParser(usage=usage)
parser.add_option("-s", "--server", dest="server", action="store",
                  default='127.0.0.1',
                  help="server address [default: %default]")
parser.add_option("-i", "--iteration", dest="iteration", action="store",
                  default=1,
                  help="number of iteration on cache miss [default: %default]")
parser.add_option("-H", "--cache-hit", dest="cache_hit", action="store",
                  default=90,
                  help="cache hit rate (0-100) [default: %default]")
parser.add_option("-r", "--response-port", dest="resp_port", action="store",
                  default=5301,
                  help="port for receiving responses [default: %default]")
parser.add_option("-n", "--packets", dest="n_packets", action="store",
                  default=1000000,
                  help="number of pakcets to be sent [default: %default]")
(options, args) = parser.parse_args()

random.random()

QUERY_PORT = 5300
QUERY_NAME = 'www.example.com'

renderer = MessageRenderer()

# Create query message
msg = Message(Message.RENDER)
msg.set_qid(0)
msg.set_rcode(Rcode.NOERROR())
msg.set_opcode(Opcode.QUERY())
msg.add_question(Question(Name(QUERY_NAME), RRClass.IN(), RRType.A()))
msg.to_wire(renderer)
query_data = renderer.get_data()

# make sure the first byte will be non 0, indicating need for recursion
msg.set_qid(0x8000)
msg.to_wire(renderer)
query_nocache_data = renderer.get_data()

# Create referral response message at one-level higher
msg.clear(Message.RENDER)
renderer.clear()
# make sure the first byte will be non 0, indicating need for another iteration
msg.set_qid(0x8000)
msg.set_rcode(Rcode.NOERROR())
msg.set_opcode(Opcode.QUERY())
msg.set_header_flag(Message.HEADERFLAG_QR)
msg.add_question(Question(Name('www.example.com'), RRClass.IN(),
                          RRType.A()))
auth_ns = RRset(Name('example.com'), RRClass.IN(), RRType.NS(), RRTTL(172800))
auth_ns.add_rdata(Rdata(RRType.NS(), RRClass.IN(), 'ns1.example.com'))
auth_ns.add_rdata(Rdata(RRType.NS(), RRClass.IN(), 'ns2.example.com'))
auth_ns.add_rdata(Rdata(RRType.NS(), RRClass.IN(), 'ns3.example.com'))
auth_ns.add_rdata(Rdata(RRType.NS(), RRClass.IN(), 'ns4.example.com'))
msg.add_rrset(Message.SECTION_AUTHORITY, auth_ns)

additional_a = RRset(Name('ns1.example.com'), RRClass.IN(), RRType.A(),
                     RRTTL(172800))
additional_a.add_rdata(Rdata(RRType.A(), RRClass.IN(), '192.0.2.1'))
msg.add_rrset(Message.SECTION_ADDITIONAL, additional_a)
additional_a = RRset(Name('ns2.example.com'), RRClass.IN(), RRType.A(),
                     RRTTL(172800))
additional_a.add_rdata(Rdata(RRType.A(), RRClass.IN(), '192.0.2.2'))
msg.add_rrset(Message.SECTION_ADDITIONAL, additional_a)
additional_a = RRset(Name('ns3.example.com'), RRClass.IN(), RRType.A(),
                     RRTTL(172800))
additional_a.add_rdata(Rdata(RRType.A(), RRClass.IN(), '192.0.2.3'))
msg.add_rrset(Message.SECTION_ADDITIONAL, additional_a)
additional_a = RRset(Name('ns4.example.com'), RRClass.IN(), RRType.A(),
                     RRTTL(172800))
additional_a.add_rdata(Rdata(RRType.A(), RRClass.IN(), '192.0.2.4'))
msg.add_rrset(Message.SECTION_ADDITIONAL, additional_a)
msg.to_wire(renderer)
referral_data = renderer.get_data()

# Create final response message
msg.clear(Message.RENDER)
renderer.clear()
msg.set_qid(0)
msg.set_rcode(Rcode.NOERROR())
msg.set_opcode(Opcode.QUERY())
msg.set_header_flag(Message.HEADERFLAG_QR)
msg.set_header_flag(Message.HEADERFLAG_AA)
msg.add_question(Question(Name('www.example.com'), RRClass.IN(),
                          RRType.A()))
answer_cname = RRset(Name(QUERY_NAME), RRClass.IN(), RRType.CNAME(),
                     RRTTL(604800))
answer_cname.add_rdata(Rdata(RRType.CNAME(), RRClass.IN(),
                             'www.l.example.com'))
answer_a = RRset(Name('www.l.example.com'), RRClass.IN(), RRType.A(),
                 RRTTL(300))
answer_a.add_rdata(Rdata(RRType.A(), RRClass.IN(), '192.0.2.1'))
answer_a.add_rdata(Rdata(RRType.A(), RRClass.IN(), '192.0.2.2'))
answer_a.add_rdata(Rdata(RRType.A(), RRClass.IN(), '192.0.2.3'))
answer_a.add_rdata(Rdata(RRType.A(), RRClass.IN(), '192.0.2.4'))
answer_a.add_rdata(Rdata(RRType.A(), RRClass.IN(), '192.0.2.5'))
msg.add_rrset(Message.SECTION_ANSWER, answer_cname)
msg.add_rrset(Message.SECTION_ANSWER, answer_a)
msg.to_wire(renderer)
resp_data = renderer.get_data()

query_dst = (options.server, QUERY_PORT)
resp_dst = (options.server, int(options.resp_port))
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)

query_count = 0
resp_count = 0
miss_count = 0
count_limit = int(options.n_packets)
cache_hitrate = int(options.cache_hit)
n_iter = int(options.iteration)
while query_count + resp_count < count_limit:
    # probablistically make cache miss
    r = random.randint(1, 100)
    if r <= cache_hitrate:
        sock.sendto(query_data, query_dst)
    else:
        sock.sendto(query_nocache_data, query_dst)

        miss_count += 1
        for i in range(1, n_iter):
            sock.sendto(referral_data, resp_dst)
            resp_count += 1
        sock.sendto(resp_data, resp_dst)
        resp_count += 1
        
    query_count += 1

print('total queries=' + str(query_count))
print('total packets=' + str(count_limit))
print('actual cache hit rate=%.2f%%' %
      ((query_count - miss_count) / float(query_count) * 100))
