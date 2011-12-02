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
unittest for MibfileGenerator
"""

import unittest, io
from stats import SPECFILE_LOCATION
from test_utils import MockBoss, MockAuth

class TestMibfileGenerator(unittest.TestCase):
    """Tests for StatsSnmp class"""

    def test_main(self):
        gen_module = __import__("gen-mibfile")
        gen = gen_module.MibfileGenerator()
        self.assertIsNotNone(gen)
        gen.readfiles(io.StringIO(MockBoss.spec_str),
                      io.StringIO(MockAuth.spec_str),
                      SPECFILE_LOCATION)
        self.assertTrue(gen.generate().find(MIB_STRING) >0)

MIB_STRING = """auth OBJECT IDENTIFIER ::= { statistics 0 }

authQueriestcp OBJECT-TYPE
  SYNTAX       Integer32
  MAX-ACCESS   read-only
  STATUS       current
  DESCRIPTION  "A number of total query counts which all auth servers receive over TCP since they started initially"
  ::= { auth 0 }

authQueriesudp OBJECT-TYPE
  SYNTAX       Integer32
  MAX-ACCESS   read-only
  STATUS       current
  DESCRIPTION  "A number of total query counts which all auth servers receive over UDP since they started initially"
  ::= { auth 1 }

authQueriesperzoneTable OBJECT-TYPE
  SYNTAX       SEQUENCE OF AuthZonesEntry
  MAX-ACCESS   not-accessible
  STATUS       current
  DESCRIPTION  "Queries per zone"
  ::= { auth 2 }

authZonesEntry OBJECT-TYPE
  SYNTAX       AuthZonesEntry
  MAX-ACCESS   not-accessible
  STATUS       current
  DESCRIPTION  ""
  ::= { authQueriesperzoneTable 0 }

AuthZonesEntry ::= SEQUENCE {
  authZonename  DisplayString,
  authQueriesudp  Integer32,
  authQueriestcp  Integer32
}

authZonename OBJECT-TYPE
  SYNTAX       DisplayString
  MAX-ACCESS   read-only
  STATUS       current
  DESCRIPTION  "Zonename"
  ::= { authZonesEntry 0 }

authQueriesudp OBJECT-TYPE
  SYNTAX       Integer32
  MAX-ACCESS   read-only
  STATUS       current
  DESCRIPTION  "A number of UDP query counts per zone"
  ::= { authZonesEntry 1 }

authQueriestcp OBJECT-TYPE
  SYNTAX       Integer32
  MAX-ACCESS   read-only
  STATUS       current
  DESCRIPTION  "A number of TCP query counts per zone"
  ::= { authZonesEntry 2 }

boss OBJECT IDENTIFIER ::= { statistics 1 }

bossBoottime OBJECT-TYPE
  SYNTAX       DisplayString
  MAX-ACCESS   read-only
  STATUS       current
  DESCRIPTION  "A date time when bind10 process starts initially"
  ::= { boss 0 }

stats OBJECT IDENTIFIER ::= { statistics 2 }

statsReporttime OBJECT-TYPE
  SYNTAX       DisplayString
  MAX-ACCESS   read-only
  STATUS       current
  DESCRIPTION  "A date time when stats module reports"
  ::= { stats 0 }

statsBoottime OBJECT-TYPE
  SYNTAX       DisplayString
  MAX-ACCESS   read-only
  STATUS       current
  DESCRIPTION  "A date time when the stats module starts initially or when the stats module restarts"
  ::= { stats 1 }

statsLastupdatetime OBJECT-TYPE
  SYNTAX       DisplayString
  MAX-ACCESS   read-only
  STATUS       current
  DESCRIPTION  "The latest date time when the stats module receives from other modules like auth server or boss process and so on"
  ::= { stats 2 }

statsTimestamp OBJECT-TYPE
  SYNTAX       TimeTicks
  MAX-ACCESS   read-only
  STATUS       current
  DESCRIPTION  "A current time stamp since epoch time (1970-01-01T00:00:00Z)"
  ::= { stats 3 }

statsLname OBJECT-TYPE
  SYNTAX       DisplayString
  MAX-ACCESS   read-only
  STATUS       current
  DESCRIPTION  "A localname of stats module given via CC protocol"
  ::= { stats 4 }
"""

if __name__ == "__main__":
    unittest.main()
