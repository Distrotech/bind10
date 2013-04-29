if HAVE_GTK
gbindctl = gbindctl
endif

SUBDIRS = bind10 bindctl $(gbindctl) cfgmgr ddns loadzone msgq cmdctl auth \
	xfrin xfrout usermgr zonemgr stats tests resolver sockcreator \
	dhcp4 dhcp6 dbutil sysinfo

check-recursive: all-recursive
