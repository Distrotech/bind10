if ENABLE_bind10
bind10 = bind10
endif

if ENABLE_bindctl
bindctl = bindctl
endif

if ENABLE_cfgmgr
cfgmgr = cfgmgr
endif

# This isn't ideal but the auth tests need it
if ENABLE_auth
cfgmgr = cfgmgr
endif

if ENABLE_ddns
ddns = ddns
endif

if ENABLE_loadzone
loadzone = loadzone
endif

if ENABLE_msgq
msgq = msgq
endif

if ENABLE_cmdctl
cmdctl = cmdctl
endif

if ENABLE_auth
auth = auth
endif

if ENABLE_xfrin
xfrin = xfrin
endif

if ENABLE_xfrout
xfrout = xfrout
endif

if ENABLE_usermgr
usermgr = usermgr
endif

if ENABLE_statistics
statistics = stats
endif

if ENABLE_resolver
resolver = resolver
endif

if ENABLE_sockcreator
sockcreator = sockcreator
endif

if ENABLE_dhcp4
dhcp4 = dhcp4
bind10 = bind10
endif

if ENABLE_dhcp6
dhcp6 = dhcp6
bind10 = bind10
endif

if ENABLE_d2
d2 = d2
bind10 = bind10
endif

if ENABLE_dbutil
dbutil = dbutil
endif

if ENABLE_sysinfo
sysinfo = sysinfo
endif

if ENABLE_zonemgr
zonemgr = zonemgr
endif

if ENABLE_memmgr
if USE_SHARED_MEMORY
memmgr = memmgr
endif
endif

SUBDIRS = $(bind10) $(bindctl) $(cfgmgr) $(ddns) $(loadzone) $(msgq) \
	$(cmdctl) $(auth) $(xfrin) $(xfrout) $(usermgr) $(zonemgr) \
	$(statistics) $(resolver) $(sockcreator) $(dhcp4) $(dhcp6) \
	$(d2) $(dbutil) $(sysinfo) $(memmgr) \
	tests

check-recursive: all-recursive
