if ENABLE_dnslibs
acl = acl
ddns = ddns
datasrc = datasrc
endif

if ENABLE_memmgr
if USE_SHARED_MEMORY
memmgr = memmgr
endif
endif

if ENABLE_xfrin
xfrin = xfrin
datasrc = datasrc
notify = notify
srvcommon = server_common
acl = acl
endif

if ENABLE_xfrout
acl = acl
endif

if ENABLE_auth
acl = acl
datasrc = datasrc
srvcommon = server_common
xfrin = xfrin
endif

if ENABLE_sysinfo
sysinfo = sysinfo
endif

if ENABLE_statistics
statistics = statistics
endif

if ENABLE_bind10
bind10 = bind10
endif

if ENABLE_libdns
libdns = dns
endif

util = util
ccdir = cc
config = config
log = log
net = net
testutils = testutils
logmsg = log_messages

SUBDIRS = $(datasrc) $(util) $(ccdir) $(config) $(libdns) $(log) $(net) \
	$(notify) $(testutils) $(acl) $(bind10) $(xfrin) $(logmsg) \
	$(srvcommon) $(ddns) $(sysinfo) $(statistics) $(memmgr)

python_PYTHON = __init__.py

pythondir = $(pyexecdir)/isc

CLEANDIRS = __pycache__

clean-local:
	rm -rf $(CLEANDIRS)
