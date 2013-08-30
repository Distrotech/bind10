Feature: Basic DHCP
    This feature set is just testing the execution of the b10-dhcp4
    component. It sees whether it starts up and takes a configuration.

    Scenario: Start Dhcp4
        # This scenario starts a server that runs Dhcp4.
        When I start bind10 with configuration dhcp/dhcp4_basic.config
        And wait for bind10 stderr message BIND10_STARTED_CC
        And wait for bind10 stderr message CMDCTL_STARTED
        And wait for bind10 stderr message DHCP4_CCSESSION_STARTED
        And wait for bind10 stderr message DHCP4_DB_BACKEND_STARTED

        bind10 module Dhcp4 should be running
        And bind10 module Auth should not be running
        And bind10 module Xfrout should not be running
        And bind10 module Zonemgr should not be running
        And bind10 module Xfrin should not be running
        And bind10 module Stats should not be running
        And bind10 module StatsHttpd should not be running
