SUBDIRS = util cc config log net testutils bind10
SUBDIRS += log_messages sysinfo

if USE_SHARED_MEMORY
# The memory manager is useless without shared memory support
SUBDIRS += memmgr
endif

if ENABLE_LIBDNS
SUBDIRS += dns
endif

if ENABLE_DNS_LIBRARIES
SUBDIRS += acl datasrc ddns notify server_common
endif

if ENABLE_STATISTICS
SUBDIRS += statistics
endif

if ENABLE_XFRIN
SUBDIRS += xfrin
endif

python_PYTHON = __init__.py

pythondir = $(pyexecdir)/isc

CLEANDIRS = __pycache__

clean-local:
	rm -rf $(CLEANDIRS)
