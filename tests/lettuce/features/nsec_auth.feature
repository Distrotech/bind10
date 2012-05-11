Feature: NSEC Authoritative service
    This feature tests NSEC as defined in RFC4035, using the example
    zone from appendix A and testing the example responses from appendix B.
    Additional tests can be added as well.
    It (currently?) only uses the scenarios from appendix B that contain
    NSEC records in its responses.

    # Response section data is taken directly from RFC5155
    # It has been modified slightly; it has been 'flattened' (i.e. converted
    # to 1-line RRs with TTL and class data), and whitespace has been added
    # in the places where dig adds them too.
    # Any other changes from the specific example data are added as inline
    # comments.

    Scenario: B.2 Name Error
        Given I have bind10 running with configuration nsec/nsec_auth.config
        And wait for bind10 stderr message AUTH_SERVER_STARTED
        bind10 module Auth should be running

        A dnssec query for ml.example. should have rcode NXDOMAIN
        The last query response should have flags qr aa rd
        The last query response should have edns_flags do
        The last query response should have ancount 0
        The last query response should have nscount 6
        The last query response should have adcount 1
        The authority section of the last query response should be
        """
        example.	3600	IN	SOA	ns1.example. bugs.x.w.example. 1081539377 3600 300 3600000 3600
        example.	3600	IN	RRSIG	SOA 5 1 3600 20040509183619 20040409183619 38519 example. ONx0k36rcjaxYtcNgq6iQnpNV5+drqYAsC9h7TSJaHCqbhE67Sr6aH2x DUGcqQWu/n0UVzrFvkgO9ebarZ0GWDKcuwlM6eNB5SiX2K74l5LWDA7S /Un/IbtDq4Ay8NMNLQI7Dw7n4p8/rjkBjV7j86HyQgM5e7+miRAz8V01 b0I=
        b.example.	3600	IN	NSEC	ns1.example. NS RRSIG NSEC 
        b.example.	3600	IN	RRSIG	NSEC 5 2 3600 20040509183619 20040409183619 38519 example. GNuxHn844wfmUhPzGWKJCPY5ttEX/RfjDoOx9ueK1PtYkOWKOOdiJ/PJ KCYB3hYX+858dDWSxb2qnV/LSTCNVBnkm6owOpysY97MVj5VQEWs0lm9 tFoqjcptQkmQKYPrwUnCSNwvvclSF1xZvhRXgWT7OuFXldoCG6TfVFMs 9xE=
        example.	3600	IN	NSEC	a.example. NS SOA MX RRSIG NSEC DNSKEY 
        example.	3600	IN	RRSIG	NSEC 5 1 3600 20040509183619 20040409183619 38519 example. O0k558jHhyrC97ISHnislm4kLMW48C7U7cBmFTfhke5iVqNRVTB1STLM pgpbDIC9hcryoO0VZ9ME5xPzUEhbvGnHd5sfzgFVeGxr5Nyyq4tWSDBg IBiLQUv1ivy29vhXy7WgR62dPrZ0PWvmjfFJ5arXf4nPxp/kEowGgBRz Y/U=
        """

    Scenario: B.3 No Data Error
        Given I have bind10 running with configuration nsec/nsec_auth.config
        And wait for bind10 stderr message AUTH_SERVER_STARTED
        bind10 module Auth should be running

        A dnssec query for ns1.example. type MX should have rcode NOERROR
        The last query response should have flags qr aa rd
        The last query response should have edns_flags do
        The last query response should have ancount 0
        The last query response should have nscount 4
        The last query response should have adcount 1
        The authority section of the last query response should be
        """
        example.	3600	IN	SOA	ns1.example. bugs.x.w.example. 1081539377 3600 300 3600000 3600
        example.	3600	IN	RRSIG	SOA 5 1 3600 20040509183619 20040409183619 38519 example. ONx0k36rcjaxYtcNgq6iQnpNV5+drqYAsC9h7TSJaHCqbhE67Sr6aH2x DUGcqQWu/n0UVzrFvkgO9ebarZ0GWDKcuwlM6eNB5SiX2K74l5LWDA7S /Un/IbtDq4Ay8NMNLQI7Dw7n4p8/rjkBjV7j86HyQgM5e7+miRAz8V01 b0I=
        ns1.example.	3600	IN	NSEC	ns2.example. A RRSIG NSEC 
        ns1.example.	3600	IN	RRSIG	NSEC 5 2 3600 20040509183619 20040409183619 38519 example. I4hj+Kt6+8rCcHcUdolks2S+Wzri9h3fHas81rGN/eILdJHN7JpV6lLG PIh/8fIBkfvdyWnBjjf1q3O7JgYO1UdI7FvBNWqaaEPJK3UkddBqZIaL i8Qr2XHkjq38BeQsbp8X0+6h4ETWSGT8IZaIGBLryQWGLw6Y6X8dqhln xJM=
        """

    Scenario: B.5 Referral to unsigned zone
        Given I have bind10 running with configuration nsec/nsec_auth.config
        And wait for bind10 stderr message AUTH_SERVER_STARTED
        bind10 module Auth should be running

        A dnssec query for mc.b.example. type MX should have rcode NOERROR
        The last query response should have flags qr rd
        The last query response should have edns_flags do
        The last query response should have ancount 0
        The last query response should have nscount 4
        The last query response should have adcount 3
        The authority section of the last query response should be
        """
        b.example.	3600	IN	NS	ns1.b.example.
        b.example.	3600	IN	NS	ns2.b.example.
        b.example.	3600	IN	NSEC	ns1.example. NS RRSIG NSEC 
        b.example.	3600	IN	RRSIG	NSEC 5 2 3600 20040509183619 20040409183619 38519 example. GNuxHn844wfmUhPzGWKJCPY5ttEX/RfjDoOx9ueK1PtYkOWKOOdiJ/PJ KCYB3hYX+858dDWSxb2qnV/LSTCNVBnkm6owOpysY97MVj5VQEWs0lm9 tFoqjcptQkmQKYPrwUnCSNwvvclSF1xZvhRXgWT7OuFXldoCG6TfVFMs 9xE=
        """
        The additional section of the last query response should be
        """
        ns1.b.example. 3600 IN A   192.0.2.7
        ns2.b.example. 3600 IN A   192.0.2.8
        """

    Scenario: B.6 Wildcard expansion
        Given I have bind10 running with configuration nsec/nsec_auth.config
        And wait for bind10 stderr message AUTH_SERVER_STARTED
        bind10 module Auth should be running

        A dnssec query for a.z.w.example. type MX should have rcode NOERROR
        The last query response should have flags qr aa rd
        The last query response should have edns_flags do
        The last query response should have ancount 2
        The last query response should have nscount 5
        The last query response should have adcount 9
        The answer section of the last query response should be
        """
        a.z.w.example.	3600	IN	MX	1 ai.example.
        a.z.w.example.	3600	IN	RRSIG	MX 5 2 3600 20040509183619 20040409183619 38519 example. OMK8rAZlepfzLWW75Dxd63jy2wswESzxDKG2f9AMN1CytCd10cYISAxf AdvXSZ7xujKAtPbctvOQ2ofO7AZJ+d01EeeQTVBPq4/6KCWhqe2XTjnk VLNvvhnc0u28aoSsG0+4InvkkOHknKxw4kX18MMR34i8lC36SR5xBni8 vHI=
        """
        The authority section of the last query response should be
        """
        example.	3600	IN	NS	ns1.example.
        example.	3600	IN	NS	ns2.example.
        example.	3600	IN	RRSIG	NS 5 1 3600 20040509183619 20040409183619 38519 example. gl13F00f2U0R+SWiXXLHwsMY+qStYy5k6zfdEuivWc+wd1fmbNCyql0T k7lHTX6UOxc8AgNf4ISFve8XqF4q+o9qlnqIzmppU3LiNeKT4FZ8RO5u rFOvoMRTbQxW3U0hXWuggE4g3ZpsHv480HjMeRaZB/FRPGfJPajngcq6 Kwg=
        x.y.w.example.	3600	IN	NSEC	xx.example. MX RRSIG NSEC 
        x.y.w.example.	3600	IN	RRSIG	NSEC 5 4 3600 20040509183619 20040409183619 38519 example. OvE6WUzN2ziieJcvKPWbCAyXyP6ef8cr6CspArVSTzKSquNwbezZmkU7 E34o5lmb6CWSSSpgxw098kNUFnHcQf/LzY2zqRomubrNQhJTiDTXa0Ar unJQCzPjOYq5t0SLjm6qp6McJI1AP5VrQoKqJDCLnoAlcPOPKAm/jJkn 3jk=
        """
        # Note that our additional section is different from the example
        # in the RFC; we also include NS address data.
        The additional section of the last query response should be
        """
        ai.example.		3600	IN	A	192.0.2.9
        ai.example.		3600	IN	RRSIG	A 5 2 3600 20040509183619 20040409183619 38519 example. pAOtzLP2MU0tDJUwHOKE5FPIIHmdYsCgTb5BERGgpnJluA9ixOyf6xxV CgrEJW0WNZSsJicdhBHXfDmAGKUajUUlYSAH8tS4ZnrhyymIvk3uArDu 2wfT130e9UHnumaHHMpUTosKe22PblOy6zrTpg9FkS0XGVmYRvOTNYx2 HvQ=
        ai.example.		3600	IN	AAAA	2001:db8::f00:baa9
        ai.example.		3600	IN	RRSIG	AAAA 5 2 3600 20040509183619 20040409183619 38519 example. nLcpFuXdT35AcE+EoafOUkl69KB+/e56XmFKkewXG2IadYLKAOBIoR5+ VoQV3XgTcofTJNsh1rnF6Eav2zpZB3byI6yo2bwY8MNkr4A7cL9TcMmD wV/hWFKsbGBsj8xSCN/caEL2CWY/5XP2sZM6QjBBLmukH30+w1z3h8PU P2o=
        ns1.example.		3600	IN	A	192.0.2.1
        ns1.example.		3600	IN	RRSIG	A 5 2 3600 20040509183619 20040409183619 38519 example. F1C9HVhIcs10cZU09G5yIVfKJy5yRQQ3qVet5pGhp82pzhAOMZ3K22Jn mK4c+IjUeFp/to06im5FVpHtbFisdjyPq84bhTv8vrXt5AB1wNB++iAq vIfdgW4sFNC6oADb1hK8QNauw9VePJhKv/iVXSYC0b7mPSU+EOlknFpV ECs=
        ns2.example.		3600	IN	A	192.0.2.2
        ns2.example.		3600	IN	RRSIG	A 5 2 3600 20040509183619 20040409183619 38519 example. V7cQRw1TR+knlaL1z/psxlS1PcD37JJDaCMqQo6/u1qFQu6x+wuDHRH2 2Ap9ulJPQjFwMKOuyfPGQPC8KzGdE3vt5snFEAoE1Vn3mQqtu7SO6amI jk13Kj/jyJ4nGmdRIc/3cM3ipXFhNTKqrdhx8SZ0yy4ObIRzIzvBFLiS S8o=
        """

    Scenario: B.7 Wildcard No Data Error
        Given I have bind10 running with configuration nsec/nsec_auth.config
        And wait for bind10 stderr message AUTH_SERVER_STARTED
        bind10 module Auth should be running

        A dnssec query for a.z.w.example. type AAAA should have rcode NOERROR
        The last query response should have flags qr aa rd
        The last query response should have edns_flags do
        The last query response should have ancount 0
        The last query response should have nscount 6
        The last query response should have adcount 1
        The authority section of the last query response should be
        """
        example.	3600	IN	SOA	ns1.example. bugs.x.w.example. 1081539377 3600 300 3600000 3600
        example.	3600	IN	RRSIG	SOA 5 1 3600 20040509183619 20040409183619 38519 example. ONx0k36rcjaxYtcNgq6iQnpNV5+drqYAsC9h7TSJaHCqbhE67Sr6aH2x DUGcqQWu/n0UVzrFvkgO9ebarZ0GWDKcuwlM6eNB5SiX2K74l5LWDA7S /Un/IbtDq4Ay8NMNLQI7Dw7n4p8/rjkBjV7j86HyQgM5e7+miRAz8V01 b0I=
        x.y.w.example.	3600	IN	NSEC	xx.example. MX RRSIG NSEC 
        x.y.w.example.	3600	IN	RRSIG	NSEC 5 4 3600 20040509183619 20040409183619 38519 example. OvE6WUzN2ziieJcvKPWbCAyXyP6ef8cr6CspArVSTzKSquNwbezZmkU7 E34o5lmb6CWSSSpgxw098kNUFnHcQf/LzY2zqRomubrNQhJTiDTXa0Ar unJQCzPjOYq5t0SLjm6qp6McJI1AP5VrQoKqJDCLnoAlcPOPKAm/jJkn 3jk=
        *.w.example.	3600	IN	NSEC	x.w.example. MX RRSIG NSEC 
        *.w.example.	3600	IN	RRSIG	NSEC 5 2 3600 20040509183619 20040409183619 38519 example. r/mZnRC3I/VIcrelgIcteSxDhtsdlTDt8ng9HSBlABOlzLxQtfgTnn8f +aOwJIAFe1Ee5RvU5cVhQJNP5XpXMJHfyps8tVvfxSAXfahpYqtx91gs mcV/1V9/bZAG55CefP9cM4Z9Y9NT9XQ8s1InQ2UoIv6tJEaaKkP701j8 OLA=
        """

    Scenario: B.8 DS Child Zone No Data Error
        Given I have bind10 running with configuration nsec/nsec_auth.config
        And wait for bind10 stderr message AUTH_SERVER_STARTED
        bind10 module Auth should be running

        A dnssec query for example. type DS should have rcode NOERROR
        The last query response should have flags qr aa rd
        The last query response should have edns_flags do
        The last query response should have ancount 0
        The last query response should have nscount 4
        The last query response should have adcount 1
        The authority section of the last query response should be
        """
        example.	3600	IN	SOA	ns1.example. bugs.x.w.example. 1081539377 3600 300 3600000 3600
        example.	3600	IN	RRSIG	SOA 5 1 3600 20040509183619 20040409183619 38519 example. ONx0k36rcjaxYtcNgq6iQnpNV5+drqYAsC9h7TSJaHCqbhE67Sr6aH2x DUGcqQWu/n0UVzrFvkgO9ebarZ0GWDKcuwlM6eNB5SiX2K74l5LWDA7S /Un/IbtDq4Ay8NMNLQI7Dw7n4p8/rjkBjV7j86HyQgM5e7+miRAz8V01 b0I=
        example.	3600	IN	NSEC	a.example. NS SOA MX RRSIG NSEC DNSKEY 
        example.	3600	IN	RRSIG	NSEC 5 1 3600 20040509183619 20040409183619 38519 example. O0k558jHhyrC97ISHnislm4kLMW48C7U7cBmFTfhke5iVqNRVTB1STLM pgpbDIC9hcryoO0VZ9ME5xPzUEhbvGnHd5sfzgFVeGxr5Nyyq4tWSDBg IBiLQUv1ivy29vhXy7WgR62dPrZ0PWvmjfFJ5arXf4nPxp/kEowGgBRz Y/U=
        """

