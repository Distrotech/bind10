. ${BIND10HOME}/win32build/env-debug.sh
echo
echo skipping dhcp6-tests / Debug
echo
exit 0
${BIND10HOME}/win32build/VS/dhcp6-tests/Debug/dhcp6_unittests.exe
if test $? -ne 0; then
    exit -1
fi
if [ ${PYTHON} == "None" ]; then
    echo skipping dhcp6 python / Debug
    exit 0
fi
echo skipping dhcp6 python / Debug
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/lib/python/isc/log_messages"
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/lib/python"
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/bin"
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/bin/bind10"
PYTESTS='dhcp6_test.py'
#for pytest in ${PYTESTS}
#    do
#        echo ${pytest}
#        ${PYTHON} ${BIND10HOME}/src/bin/dhcp6/tests/${pytest}
#        if test $? -ne 0; then
#            exit -1
#        fi
#    done
