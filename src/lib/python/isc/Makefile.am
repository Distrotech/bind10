
if ENABLE_DNS_LIBRARIES
# ddns module uses xfrin
acl = acl
ddns = acl ddns
datasrc = datasrc
if USE_SHARED_MEMORY
# The memory manager is useless without shared memory support
memmgr = memmgr
endif
notify = notify
server_common = server_common
xfrin = xfrin
endif

if ENABLE_XFRIN
datasrc = datasrc
if USE_SHARED_MEMORY
# The memory manager is useless without shared memory support
memmgr = memmgr
endif
notify = notify
server_common = server_common
xfrin = xfrin
endif

if ENABLE_XFROUT
acl = acl
endif

SUBDIRS = util cc config log net testutils bind10 $(datasrc) $(xfrin)
SUBDIRS += $(acl) $(ddns) $(server_common) $(notify) $(memmgr)
SUBDIRS += log_messages sysinfo 

if ENABLE_LIBDNS
SUBDIRS += dns
endif

if ENABLE_STATISTICS
SUBDIRS += statistics
endif

python_PYTHON = __init__.py

pythondir = $(pyexecdir)/isc

CLEANDIRS = __pycache__

clean-local:
	rm -rf $(CLEANDIRS)
