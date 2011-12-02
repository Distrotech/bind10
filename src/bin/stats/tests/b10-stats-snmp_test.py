# Copyright (C) 2011  Internet Systems Consortium.
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

"""
unittest for StatsSnmp
"""

import unittest, io, os, sys
import stats_snmp
from stats_snmp import oid2str, str2oid, BASE_OID
from test_utils import BaseModules, ThreadingServerManager, MyStats, SignalHandler

class TestStatsSnmp(unittest.TestCase):
    """Tests for StatsSnmp class"""

    def setUp(self):
        # set the signal handler for deadlock
        self.sig_handler = SignalHandler(self.fail)
        self.base = BaseModules()
        self.stats_server = ThreadingServerManager(MyStats)
        self.stats_server.run()

    def tearDown(self):
        self.stats_server.shutdown()
        self.base.shutdown()
        # reset the signal handler
        self.sig_handler.reset()

    def test_PING(self):
        strin = os.popen('echo PING')
        strout = io.StringIO()
        stats_snmp.sys.stdin = strin
        stats_snmp.sys.stdout = strout
        snmp = stats_snmp.StatsSnmp()
        snmp.start()
        self.assertEqual(strout.getvalue(), 'PONG\n')
        strin.close()
        strout.close()

    def test_NONE(self):
        strin = os.popen('echo FOO')
        strout = io.StringIO()
        stats_snmp.sys.stdin = strin
        stats_snmp.sys.stdout = strout
        snmp = stats_snmp.StatsSnmp()
        snmp.start()
        self.assertEqual(strout.getvalue(), 'NONE\n')
        strin.close()
        strout.close()

    def test_get(self):
        snmp = stats_snmp.StatsSnmp()
        oid = '%s.0.0' % oid2str(BASE_OID)
        strin = os.popen('echo get; echo %s' % oid)
        strout = io.StringIO()
        stats_snmp.sys.stdin = strin
        stats_snmp.sys.stdout = strout
        snmp.start()
        mibdata = snmp.getMibdata()
        oid = str2oid(oid)
        self.assertTrue(oid in mibdata)
        self.assertTrue('type' in mibdata[oid])
        self.assertTrue('value' in mibdata[oid])
        self.assertEqual(mibdata[oid]['type'], 'integer')
        self.assertEqual(mibdata[oid]['value'], 0)
        self.assertEqual(strout.getvalue(),
                         '%s\n%s\n%s\n'
                         % (oid2str(oid),
                            mibdata[oid]['type'],
                            mibdata[oid]['value']))
        strin.close()
        strout.close()

        snmp = stats_snmp.StatsSnmp()
        oid = '%s.0.0' % oid2str(BASE_OID)
        strin = os.popen('echo get; echo %s' % oid)
        strout = io.StringIO()
        stats_snmp.sys.stdin = strin
        stats_snmp.sys.stdout = strout
        snmp.start()
        mibdata = snmp.getMibdata()
        oid = str2oid(oid)
        self.assertTrue(oid in mibdata)
        self.assertTrue('type' in mibdata[oid])
        self.assertTrue('value' in mibdata[oid])
        self.assertEqual(mibdata[oid]['type'], 'integer')
        self.assertEqual(mibdata[oid]['value'], 0)
        self.assertEqual(strout.getvalue(),
                         '%s\n%s\n%s\n'
                         % (oid2str(oid),
                            mibdata[oid]['type'],
                            mibdata[oid]['value']))
        strin.close()
        strout.close()

        # test end of oid
        snmp = stats_snmp.StatsSnmp()
        mibdata = snmp.getMibdata()
        oid = oid2str(sorted(mibdata.keys())[-1])
        strin = os.popen('echo get; echo %s' % oid)
        strout = io.StringIO()
        stats_snmp.sys.stdin = strin
        stats_snmp.sys.stdout = strout
        snmp.start()
        oid = str2oid(oid)
        self.assertTrue(oid in mibdata)
        self.assertTrue('type' in mibdata[oid])
        self.assertTrue('value' in mibdata[oid])
        self.assertEqual(mibdata[oid]['type'], 'string')
        self.assertTrue(mibdata[oid]['value'].find('@') > 0)
        self.assertEqual(strout.getvalue(),
                         '%s\n%s\n%s\n'
                         % (oid2str(oid),
                            mibdata[oid]['type'],
                            mibdata[oid]['value']))
        strin.close()
        strout.close()

    def test_getnext(self):
        snmp = stats_snmp.StatsSnmp()
        oid = '%s' % oid2str(BASE_OID)
        strin = os.popen('echo getnext; echo %s' % oid)
        strout = io.StringIO()
        stats_snmp.sys.stdin = strin
        stats_snmp.sys.stdout = strout
        snmp.start()
        noid = str2oid('%s.0.0' % oid)
        mibdata = snmp.getMibdata()
        self.assertTrue(noid in mibdata)
        self.assertTrue('type' in mibdata[noid])
        self.assertTrue('value' in mibdata[noid])
        self.assertEqual(mibdata[noid]['type'], 'integer')
        self.assertEqual(mibdata[noid]['value'], 0)
        self.assertEqual(strout.getvalue(),
                         '%s\n%s\n%s\n'
                         % (oid2str(noid),
                            mibdata[noid]['type'],
                            mibdata[noid]['value']))
        strin.close()
        strout.close()

        snmp = stats_snmp.StatsSnmp()
        oid = '%s.0.0' % oid2str(BASE_OID)
        strin = os.popen('echo getnext; echo %s' % oid)
        strout = io.StringIO()
        stats_snmp.sys.stdin = strin
        stats_snmp.sys.stdout = strout
        snmp.start()
        noid = str2oid('%s.0.1' % oid2str(BASE_OID))
        mibdata = snmp.getMibdata()
        self.assertTrue(noid in mibdata)
        self.assertTrue('type' in mibdata[noid])
        self.assertTrue('value' in mibdata[noid])
        self.assertEqual(mibdata[noid]['type'], 'integer')
        self.assertEqual(mibdata[noid]['value'], 0)
        self.assertEqual(strout.getvalue(),
                         '%s\n%s\n%s\n'
                         % (oid2str(noid),
                            mibdata[noid]['type'],
                            mibdata[noid]['value']))
        strin.close()
        strout.close()

        # test end of oid
        snmp = stats_snmp.StatsSnmp()
        mibdata = snmp.getMibdata()
        oid = oid2str(sorted(mibdata.keys())[-1])
        strin = os.popen('echo getnext; echo %s' % oid)
        strout = io.StringIO()
        stats_snmp.sys.stdin = strin
        stats_snmp.sys.stdout = strout
        snmp.start()
        self.assertEqual(strout.getvalue(), 'NONE\n')
        strin.close()
        strout.close()

if __name__ == "__main__":
    unittest.main()
