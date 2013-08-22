SUBDIRS = bind10 bindctl cfgmgr msgq cmdctl \
	usermgr tests \
	sockcreator dbutil sysinfo

if ENABLE_DHCP
SUBDIRS += dhcp4 dhcp6 d2
endif

if ENABLE_AUTH
SUBDIRS += auth
endif

if ENABLE_DDNS
SUBDIRS += ddns
endif

if ENABLE_LOADZONE
SUBDIRS += loadzone
endif

if ENABLE_STATISTICS
SUBDIRS += stats
endif

if ENABLE_XFRIN
SUBDIRS += xfrin
endif

if ENABLE_XFROUT
SUBDIRS += xfrout
endif

if ENABLE_ZONEMGR
SUBDIRS += zonemgr
endif

if ENABLE_RESOLVER
# Build resolver only with --enable-experimental-resolver
SUBDIRS += resolver
endif

if ENABLE_STATISTICS
SUBDIRS += stats
endif

if USE_SHARED_MEMORY
# Build the memory manager only if we have shared memory.
# It is useless without it.
SUBDIRS += memmgr
endif

check-recursive: all-recursive
