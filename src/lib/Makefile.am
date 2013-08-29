SUBDIRS = exceptions util log hooks cryptolink cc config \
          asiolink testutils \
          python statistics

if ENABLE_LIBDNS
SUBDIRS += dns
endif

if ENABLE_AUTH
# nsas is used by query_bench
SUBDIRS += dns acl nsas bench asiodns datasrc server_common xfr
endif

if ENABLE_LIBDHCP
# asiodns is used by b10-dhcp-ddns
SUBDIRS += asiodns dhcp dhcp_ddns dhcpsrv
endif

if ENABLE_RESOLVER
SUBDIRS += nsas asiodns acl server_common cache resolve
endif

if ENABLE_STATISTICS
SUBDIRS += statistics
endif
