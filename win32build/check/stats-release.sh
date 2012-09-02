. ${BIND10HOME}/win32build/env-release.sh
echo
echo skipping stats-tests / Release
echo
exit 0
if [ ${PYTHON} == "None" ]; then
    echo skipping stats python / Release
    exit 0
fi
export TESTDATA_PATH=${BIND10HOME}/src/bin/stats/tests/testdata
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/lib/python/isc/log_messages"
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/lib/python"
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/bin/stats"
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/bin/stats/tests"
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/bin/msgq"
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/lib/python/isc/config"
export B10_FROM_SOURCE=${BIND10HOME}
export BIND10_MSGQ_SOCKET_FILE=${BIND10HOME}/msgq_socket
export CONFIG_TESTDATA_PATH=${BIND10HOME}/src/lib/config/tests/testdata
export B10_LOCKFILE_DIR_FROM_BUILD=${BIND10HOME}
PYTESTS='b10-stats_test.py b10-stats-httpd_test.py'
for pytest in ${PYTESTS}
    do
        echo ${pytest}
        ${PYTHON} ${BIND10HOME}/src/bin/stats/tests/${pytest}
        if test $? -ne 0; then
            exit -1
        fi
    done
