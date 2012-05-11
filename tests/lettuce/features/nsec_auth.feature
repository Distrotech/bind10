Feature: NSEC Authoritative service
    This feature tests NSEC as defined in RFC4035, using the example
    zone from appendix A and testing the example responses from appendix B.
    Additional tests can be added as well.

    # Response section data is taken directly from RFC5155
    # It has been modified slightly; it has been 'flattened' (i.e. converted
    # to 1-line RRs with TTL and class data), and whitespace has been added
    # in the places where dig adds them too.
    # Any other changes from the specific example data are added as inline
    # comments.

    Scenario: 1
        Given I have bind10 running with configuration nsec/nsec_auth.config
        And wait for bind10 stderr message AUTH_SERVER_STARTED

        bind10 module Auth should be running

        #A dnssec query for a.c.x.w.example. should have rcode NXDOMAIN
        #The last query response should have flags qr aa rd
        #The last query response should have edns_flags do
        #The last query response should have ancount 0
        #The last query response should have nscount 8
        #The last query response should have adcount 1
        #The authority section of the last query response should be
        #"""
        #"""


