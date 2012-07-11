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

import re
import sys
from isc.dns import *
from optparse import OptionParser

# ssss.mmm ip_addr#port qname qclass qtype
RE_LOGLINE = re.compile(r'^([\d\.]*) ([\d\.]*)#\d+ (\S*) (\S*) (\S*)$')

queries = {}

def convert_rrtype(type_txt):
    '''A helper hardcoded converter of RR type mnemonic to TYPEnnn.

    Not all standard types are supported in the isc.dns module yet,
    so this works around the gap.

    '''
    convert_db = {'KEY': 25, 'A6': 38, 'AXFR': 252, 'ANY': 255}
    if type_txt in convert_db.keys():
        return 'TYPE' + str(convert_db[type_txt])
    return type_txt

def parse_logfile(log_file):
    n_queries = 0
    with open(log_file) as log:
        for log_line in log:
            n_queries += 1
            m = re.match(RE_LOGLINE, log_line)
            if not m:
                sys.stderr.write('unexpected line: ' + log_line)
                continue
            qry_time = float(m.group(1))
            client_addr = m.group(2)
            qry_name = Name(m.group(3))
            qry_class = RRClass(m.group(4))
            qry_type = RRType(convert_rrtype(m.group(5)))
            qry_key = (qry_type, qry_name, qry_class)
            if qry_key in queries:
                queries[qry_key].append(qry_time)
            else:
                queries[qry_key] = [qry_time]
    return n_queries

def dump_stat(total_queries, qry_params, dump_file):
    cumulative_n_qry = 0
    position = 1
    with open(dump_file, 'w') as f:
        for qry_param in qry_params:
            n_queries = len(queries[qry_param])
            cumulative_n_qry += n_queries
            cumulative_percentage = \
                (float(cumulative_n_qry) / total_queries) * 100
            f.write('%d,%.2f\n' % (position, cumulative_percentage))
            position += 1

def dump_queries(qry_params, dump_file):
    with open(dump_file, 'w') as f:
        for qry_param in qry_params:
            f.write('%d/%s/%s/%s\n' % (len(queries[qry_param]),
                                       str(qry_param[2]), str(qry_param[0]),
                                       qry_param[1]))

def main(log_file, options):
    total_queries = parse_logfile(log_file)
    print('total_queries=%d, unique queries=%d' %
          (total_queries, len(queries)))
    qry_params = list(queries.keys())
    qry_params.sort(key=lambda x: -len(queries[x]))
    if options.popularity_file:
        dump_stat(total_queries, qry_params, options.popularity_file)
    if options.dump_file:
        dump_queries(qry_params, options.dump_file)

def get_option_parser():
    parser = OptionParser(usage='usage: %prog [options] log_file')
    parser.add_option("-p", "--dump-popularity",
                      dest="popularity_file", action="store",
                      help="dump statistics about query popularity")
    parser.add_option("-q", "--dump-queries",
                      dest="dump_file", action="store",
                      help="dump unique queries")
    return parser

if __name__ == "__main__":
    parser = get_option_parser()
    (options, args) = parser.parse_args()

    if len(args) == 0:
        parser.error('input file is missing')
    main(args[0], options)
