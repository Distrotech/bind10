if ENABLE_LIBDNS
libdns = dns
endif

if ENABLE_AUTH
asiodns = asiodns
# nsas is used by query_bench
nsas = nsas
acl = acl
auth = bench xfr
datasrc = datasrc
server_common = server_common
endif

if ENABLE_LIBDHCP
# asiodns is used by b10-dhcp-ddns
asiodns = asiodns
libdhcp = dhcp dhcp_ddns dhcpsrv
endif

if ENABLE_RESOLVER
asiodns = asiodns
nsas = nsas
acl = acl
server_common = server_common
resolver = cache resolve
endif

if ENABLE_XFRIN
datasrc = datasrc
endif

if ENABLE_XFRIN
acl = acl
endif

SUBDIRS = exceptions util log hooks cryptolink dns cc config \
          $(acl) $(auth) \
          asiolink $(asiodns) $(nsas) $(resolver) testutils \
          $(datasrc) $(server_common) python $(libdhcp)

if ENABLE_STATISTICS
SUBDIRS += statistics
endif
