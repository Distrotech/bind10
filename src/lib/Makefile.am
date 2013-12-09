if ENABLE_libdns
libdns = dns
testutils = testutils
endif

if ENABLE_botan
cryptolink = cryptolink
endif

if ENABLE_auth
asiodns = asiodns
nsas = nsas
acl = acl
bench = bench
xfr = xfr
datasrc = datasrc
srvcommon = server_common
testutils = testutils
endif

if ENABLE_libdhcp
asiodns = asiodns
libdhcp = dhcp
d2 = dhcp_ddns
dhcpsrv = dhcpsrv
endif

if ENABLE_resolver
resolver = resolve
asiodns = asiodns
nsas = nsas
acl = acl
srvcommon = server_common
cache = cache
testutils = testutils
endif

if ENABLE_xfrin
datasrc = datasrc
acl = acl
testutils = testutils
endif

if ENABLE_statistics
statistics = statistics
endif

exceptions = exceptions
util = util
log = log
hooks = hooks
ccdir = cc
config = config
asiolink = asiolink
p3libdir = python

SUBDIRS = $(exceptions) $(util) $(log) $(hooks) $(cryptolink) $(libdns) \
	$(ccdir) $(config) $(acl) $(xfr) $(bench) $(asiolink) $(asiodns) \
	$(nsas) $(cache) $(resolver) $(testutils) $(datasrc) $(srvcommon) \
	$(p3libdir) $(libdhcp) $(d2) $(dhcpsrv) $(statistics)
