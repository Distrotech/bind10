SUBDIRS = exceptions util log hooks cryptolink cc config \
          asiolink testutils \
          python statistics

if ENABLE_LIBDNS
SUBDIRS += dns
endif

if ENABLE_DNS_LIBRARIES
SUBDIRS += acl bench asiodns cache datasrc server_common
endif

if ENABLE_LIBDHCP
SUBDIRS += dhcp dhcp_ddns dhcpsrv
endif

# xfrout also?
if ENABLE_XFRIN
SUBDIRS += xfr
endif

if ENABLE_RESOLVER
SUBDIRS += nsas resolve
endif

if ENABLE_STATISTICS
SUBDIRS += statistics
endif
