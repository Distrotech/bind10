SUBDIRS = util cc config log net notify testutils acl bind10
SUBDIRS += log_messages server_common sysinfo statistics

if USE_SHARED_MEMORY
# The memory manager is useless without shared memory support
SUBDIRS += memmgr
endif

if ENABLE_LIBDNS
SUBDIRS += datasrc dns ddns
endif

if ENABLE_XFRIN
SUBDIRS += xfrin
endif

python_PYTHON = __init__.py

pythondir = $(pyexecdir)/isc

CLEANDIRS = __pycache__

clean-local:
	rm -rf $(CLEANDIRS)
