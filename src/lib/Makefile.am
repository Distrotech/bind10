SUBDIRS = exceptions util log hooks cryptolink cc config acl xfr bench \
          asiolink testutils \
          server_common python statistics

if ENABLE_LIBDNS
SUBDIRS += dns asiodns cache datasrc
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
