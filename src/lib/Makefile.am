SUBDIRS = exceptions util log hooks cryptolink dns cc config acl xfr bench \
          asiolink asiodns cache testutils \
          datasrc server_common python

if ENABLE_LIBDHCP
SUBDIRS += dhcp dhcp_ddns dhcpsrv
endif

if ENABLE_RESOLVER
SUBDIRS += nsas resolve
endif

if ENABLE_STATISTICS
SUBDIRS += statistics
endif
