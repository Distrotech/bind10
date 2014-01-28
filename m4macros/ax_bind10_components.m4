dnl @synopsis AX_BIND10_COMPONENTS
dnl
dnl Select what BIND10 components to build and install.
dnl
dnl reallyall includes experimental components
dnl
dnl Will set need_botan_conf if any of the selected components requires Botan

dnl
dnl Helper macro to ensure all --enable-component options behave the same way.
dnl Usage: AX_BIND10_COMPONENT(name,helpstring,[action-if-true],[action-if-false])
dnl
AC_DEFUN([AX_BIND10_COMPONENT],[
    AC_ARG_ENABLE($1,AS_HELP_STRING(--enable-$1,$2),[
enable_$1=$enableval
$3],$4)
    AM_CONDITIONAL(ENABLE_$1,[test "x$enable_$1" = "xyes"])
dnl    AC_SUBST(ENABLE_$1)
])

AC_DEFUN([AX_BIND10_COMPONENTS], [
need_botan_conf=no
dnl Please keep these options to single words or if you must use multiple
dnl words seperate each one with underscores, not hyphens.
dns_components="auth libdns dnslibs ddns loadzone xfrin xfrout zonemgr memmgr"
dhcp_components="dhcp4 dhcp6 d2 libdhcp"
common_components="bind10 bindctl cfgmgr msgq cmdctl usermgr sockcreator dbutil sysinfo statistics"
experimental_components="resolver"
all_components="${dns_components} ${dhcp_components} ${common_components}"
reallyall_components="${all_components} ${experimental_components}"
selected_components=

dnl Sadly autoconf doesn't allow for shell variable expansion in the help
dnl string so we have to repeat the above list.
AC_ARG_ENABLE(components,
  AS_HELP_STRING(--enable-components=FEATURES-LIST,Space-separated
list of components to enable | all | reallyall | dns | dhcp | common |
one or more of the following individual components: auth bind10
bindctl cfgmgr cmdctl d2 dbutil ddns dhcp4 dhcp6 dnslibs
libdhcp libdns loadzone memmgr msgq resolver sockcreator statistics
sysinfo usermgr xfrin xfrout zonemgr),[
    for i in $enableval; do
        if test "$i" = "yes" ; then
            i="all"
        fi
        if test "$i" = "all" ; then
            selected_components="${selected_components} ${all_components}"
        elif test "$i" = "reallyall" ; then
            selected_components="${selected_components} ${reallyall_components}"
        elif test "$i" = "dns" ; then
            selected_components="${selected_components} ${dns_components}"
        elif test "$i" = "dhcp" ; then
            selected_components="${selected_components} ${dhcp_components}"
        elif test "$i" = "common" ; then
            selected_components="${selected_components} ${common_components}"
        else
            ic=
            for j in ${reallyall_components} ; do
                if test "$i" = "$j" ; then
                    ic="$i"
                fi
            done
            if test -z "$ic" ; then
                AC_MSG_WARN(unknown component $i ignored)
            else
                selected_components="${selected_components} $i"
            fi
        fi
    done

  ],[selected_components="${all_components}"])

if test -n "${selected_components}" ; then
    components_list=`echo ${selected_components} | xargs -n 1 | sort -u | xargs`
    for i in ${components_list} ; do
        eval "enable_$i=yes"
    done
fi

dnl Set selected_components back to the empty string since we build this up
dnl again below as we process each of the individual options.
selected_components=

dnl --enable-dns meta-option
AX_BIND10_COMPONENT(dns,[Build and install all of the DNS components],[
    for i in ${dns_components} ; do
        eval "enable_$i=yes"
    done
])

dnl Now for the individual DNS components
AX_BIND10_COMPONENT(auth,[Build and install the authoritative DNS server],[
    enable_libdns=yes
    enable_loadzone=yes
    enable_memmgr=yes
    selected_components="${selected_components} auth"
])

AX_BIND10_COMPONENT(dnslibs,[Build and install the libraries required for the DNS server],[
    selected_components="${selected_components} dnslibs"
])

AX_BIND10_COMPONENT(ddns,[Build and install dynamic DNS support],[
    selected_components="${selected_components} ddns"
])

AX_BIND10_COMPONENT(loadzone,[Build and install the loadzone utility],[
    enable_memmgr=yes
    selected_components="${selected_components} loadzone"
])

AX_BIND10_COMPONENT(xfrin,[Build and install the xfrin utility],[
    enable_memmgr=yes
    selected_components="${selected_components} xfrin"
])

AX_BIND10_COMPONENT(xfrout,[Build and install the xfrout utility],[
    enable_memmgr=yes
    selected_components="${selected_components} xfrout"
])

AX_BIND10_COMPONENT(zonemgr,[Build and install the zonemgr utility],[
    enable_memmgr=yes
    selected_components="${selected_components} zonemgr"
])

dnl
dnl This option is a good lesson in autoconf option naming. Right now the
dnl resolver is experimental, but one day it won't be, and at that point the
dnl option --enable-experimental-resolver begins to stop making sense, and
dnl the natural inclination is to remove the word "experimental" from the
dnl option list. However, a lot of people may have come to depend on that
dnl name because they have build scripts that use that option, and those
dnl scripts will break if we just rename the option. So the right way of
dnl doing things is to make the option be --enable-resolver and use the
dnl help text rather than the option name to indicate its experimental
dnl status.
dnl
AC_ARG_ENABLE(experimental-resolver,
    AS_HELP_STRING(--enable-experimental-resolver,Deprecated - use --enable-resolver instead),
    [enable_resolver=$enableval])

AX_BIND10_COMPONENT(resolver,[Build and install the experimental resolver],[
    enable_libdns=yes
    selected_components="${selected_components} resolver"
])

dnl Must appear last in the list of DNS checks as other components above can
dnl enable it.
AX_BIND10_COMPONENT(memmgr,[Build and install the memmgr utility],[
    selected_components="${selected_components} memmgr"
])

dnl --enable-dhcp meta-option
AX_BIND10_COMPONENT(dhcp,[Build and install all of the DHCP components],[
    for i in ${dhcp_components} ; do
        eval "enable_$i=yes"
    done
])

AX_BIND10_COMPONENT(dhcp4,[Build and install the DHCPv4 server],[
    enable_libdhcp=yes
    selected_components="${selected_components} dhcp4"
])

AX_BIND10_COMPONENT(dhcp6,[Build and install the DHCPv6 server],[
    enable_libdhcp=yes
    selected_components="${selected_components} dhcp6"
])

AX_BIND10_COMPONENT(d2,[Build and install the DHCP-DDNS server],[
    enable_libdhcp=yes
    selected_components="${selected_components} d2"
])

AX_BIND10_COMPONENT(libdhcp,[Build and install the dhcp libraries],[
    enable_libdns=yes
    selected_components="${selected_components} libdhcp"
])

dnl --enable-common meta-option
AX_BIND10_COMPONENT(common,[Build and install components shared by DNS and DHCP],[
    for i in ${common_components} ; do
        eval "enable_$i=yes"
    done
])

AX_BIND10_COMPONENT(bind10,[Build and install the bind10 utility],[
    selected_components="${selected_components} bind10"
])

AX_BIND10_COMPONENT(bindctl,[Build and install the bindctl utility],[
    selected_components="${selected_components} bindctl"
])

AX_BIND10_COMPONENT(cfgmgr,[Build and install the configuration manager],[
    selected_components="${selected_components} cfgmgr"
])

AX_BIND10_COMPONENT(msgq,[Build and install the message queue daemon],[
    selected_components="${selected_components} msgq"
])

AX_BIND10_COMPONENT(cmdctl,[Build and install the REST API server],[
    selected_components="${selected_components} cmdctl"
])

AX_BIND10_COMPONENT(usermgr,[Build and install the user manager utility],[
    selected_components="${selected_components} usermgr"
])

AX_BIND10_COMPONENT(sockcreator,[Build and install the socket creation daemon],[
    selected_components="${selected_components} sockcreator"
])

AX_BIND10_COMPONENT(dbutil,[Build and install the BIND database utility],[
    selected_components="${selected_components} dbutil"
])

AX_BIND10_COMPONENT(sysinfo,[Build and install the ISC sysinfo utility],[
    selected_components="${selected_components} sysinfo"
])

AX_BIND10_COMPONENT(statistics,[Build and install stats components],[
    selected_components="${selected_components} statistics"
])

dnl
dnl Various tests that are order dependent (for example, one feature enables
dnl another). In general these should be grouped with the overall product as
dnl much as possible, but some tests need to be performed after both product
dnl options are done.
dnl

dnl libdns can be enabled by libdhcp and the resolver
AX_BIND10_COMPONENT(libdns,[Build and install the DNS API library],[
    selected_components="${selected_components} libdns"
])

if test "x$enable_libdns" = "xyes" ; then
    need_botan_conf=yes
fi

AM_CONDITIONAL(ENABLE_botan,[test "x$need_botan_conf" = "xyes"])

dnl If no options were given default to "all"
if test -z "$selected_components" ; then
    selected_components="$all_components"
fi

dnl Prepare the final component list.
components_list=`echo ${selected_components} | xargs -n 1 | sort -u | xargs`

])dnl AX_BIND10_COMPONENTS
