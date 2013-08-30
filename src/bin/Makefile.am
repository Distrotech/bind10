if ENABLE_DHCP
dhcp = dhcp4 dhcp6 d2
endif

if ENABLE_AUTH
auth = auth
# loadzone is used for generating test files for auth
loadzone = loadzone
endif

if ENABLE_DDNS
ddns = ddns
endif

if ENABLE_LOADZONE
loadzone = loadzone
endif

if ENABLE_RESOLVER
# Build resolver only with --enable-experimental-resolver
resolver = resolver
endif

if ENABLE_STATISTICS
stats = stats
endif

if ENABLE_XFRIN
xfrin = xfrin
endif

if ENABLE_XFROUT
xfrout = xfrout
endif

if ENABLE_ZONEMGR
zonemgr = zonemgr
endif

SUBDIRS = bind10 bindctl cfgmgr $(ddns) $(loadzone) msgq cmdctl \
        $(auth) $(xfrin) \
        $(xfrout) usermgr $(zonemgr) $(stats) tests $(resolver) \
	sockcreator $(dhcp) dbutil sysinfo

if USE_SHARED_MEMORY
# Build the memory manager only if we have shared memory.
# It is useless without it.
SUBDIRS += memmgr
endif

check-recursive: all-recursive
