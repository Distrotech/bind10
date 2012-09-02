. ${BIND10HOME}/win32build/env-debug.sh
echo
echo cfgmgr-tests / Debug
echo
if [ ${PYTHON} == "None" ]; then
    echo skipping cfgmgr python / Debug
    exit 0
fi
export TESTDATA_PATH=${BIND10HOME}/src/bin/cfgmgr/tests/testdata
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/lib/python/isc/log_messages"
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/lib/python"
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/bin/cfgmgr"
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/lib/python/isc/config"
PYTESTS='b10-cfgmgr_test.py'
for pytest in ${PYTESTS}
    do
        echo ${pytest}
        ${PYTHON} ${BIND10HOME}/src/bin/cfgmgr/tests/${pytest}
        if test $? -ne 0; then
            exit -1
        fi
    done
