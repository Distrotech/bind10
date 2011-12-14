ISC-BIND10-MIB DEFINITIONS ::= BEGIN

-- IMPORTS
IMPORTS
  OBJECT-TYPE, MODULE-IDENTITY, Integer32, Counter32, TimeTicks
    FROM SNMPv2-SMI
  TEXTUAL-CONVENTION, DisplayString, DateAndTime
    FROM SNMPv2-TC
  ucdavis
    FROM UCD-SNMP-MIB;

-- statistics Module definition
statistics MODULE-IDENTITY
  LAST-UPDATED "201112011800Z"
  ORGANIZATION "Internet Systems Consortium, Inc. (ISC)"
  CONTACT-INFO "email: bind10-dev@isc.org"
  DESCRIPTION  "Definition of Module Identity for BIND 10 statitics"
  ::= { ucdavis 255 }

$mib_string
END
