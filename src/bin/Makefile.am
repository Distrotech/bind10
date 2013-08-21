SUBDIRS = bind10 bindctl cfgmgr ddns loadzone msgq cmdctl auth xfrin \
	xfrout usermgr zonemgr stats tests \
	sockcreator dbutil sysinfo

if ENABLE_DHCP
SUBDIRS += dhcp4 dhcp6 d2
endif

if ENABLE_RESOLVER
# Build resolver only with --enable-experimental-resolver
SUBDIRS += resolver
endif

if USE_SHARED_MEMORY
# Build the memory manager only if we have shared memory.
# It is useless without it.
SUBDIRS += memmgr
endif

check-recursive: all-recursive
