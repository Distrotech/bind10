#! /bin/sh
am_version=`automake --version | head -1`

mode="parallel"
if test -n "$1" ; then
    if test "$1" = "serial" ; then
        mode="serial"
    fi
fi

case $am_version in
    *\ 1.10* | *\ 1.11*)
        mode="serial"
        echo 'm4_define([B10_TEST_HARNESS_AM], [])'
        ;;
    *\ 1.12*)
        if test "${mode}" = "parallel" ; then
            echo 'm4_define([B10_TEST_HARNESS_AM], [parallel-tests])'
        else
            echo 'm4_define([B10_TEST_HARNESS_AM], [])'
        fi
        ;;
    *)
        if test "${mode}" = "parallel" ; then
            echo 'm4_define([B10_TEST_HARNESS_AM], [])'
        else
            echo 'm4_define([B10_TEST_HARNESS_AM], [serial-tests])'
        fi
        ;;
esac > m4macros/b10_automake.m4

echo "Bootstrapping using the ${mode} test harness"

${AUTORECONF-autoreconf} -fvi
rm -f m4macros/b10_automake.m4
