. ${BIND10HOME}/win32build/env-release.sh
echo
echo partial python / Release
echo
if [ ${PYTHON} == "None" ]; then
    echo skipping python / Release
    exit 0
fi
CPYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/lib/python/isc/log_messages"
CPYTHONPATH="${CPYTHONPATH};${BIND10HOME}/src/lib/python"
CTESTDATA=${BIND10HOME}/src/lib/python/isc/datasrc/tests/testdata

echo python datasrc / Release
export PYTHONPATH="${CPYTHONPATH};${BIND10HOME}/src/lib/python/isc/log"
export TESTDATA_PATH=${CTESTDATA}
export TESTDATA_WRITE_PATH=${BIND10HOME}/src/lib/python/isc/datasrc/tests
export GLOBAL_TESTDATA_PATH=${BIND10HOME}/src/lib/testutils/testdata
PYTESTS='datasrc_test.py sqlite3_ds_test.py'
for pytest in ${PYTESTS}
    do
        echo ${pytest}
        ${PYTHON} ${BIND10HOME}/src/lib/python/isc/datasrc/tests/${pytest}
        if test $? -ne 0; then
            exit -1
        fi
    done

echo python cc / Release
export PYTHONPATH=${CPYTHONPATH}
export BIND10_TEST_SOCKET_FILE=v4_12345
PYTESTS='message_test.py data_test.py session_test.py'
for pytest in ${PYTESTS}
    do
        echo ${pytest}
        ${PYTHON} ${BIND10HOME}/src/lib/python/isc/cc/tests/${pytest}
        if test $? -ne 0; then
            exit -1
        fi
    done

echo python config / Release
export PYTHONPATH="${CPYTHONPATH};${BIND10HOME}/src/lib/python/isc/config"
export B10_LOCKFILE_DIR_FROM_BUILD=${BIND10HOME}
export B10_TEST_PLUGIN_DIR=${BIND10HOME}/src/bin/cfgmgr/plugins
export CONFIG_TESTDATA_PATH=${BIND10HOME}/src/lib/config/tests/testdata
export CONFIG_WR_TESTDATA_PATH=${BIND10HOME}/src/lib/config/tests/testdata
PYTESTS='ccsession_test.py
    cfgmgr_test.py
    config_data_test.py
    module_spec_test.py'
for pytest in ${PYTESTS}
    do
        echo ${pytest}
        ${PYTHON} ${BIND10HOME}/src/lib/python/isc/config/tests/${pytest}
        if test $? -ne 0; then
            exit -1
        fi
    done

echo python log / Release
export PYTHONPATH="${CPYTHONPATH};${BIND10HOME}/src/lib/python/isc/log"
export B10_LOCKFILE_DIR_FROM_BUILD=${BIND10HOME}
export B10_TEST_PLUGIN_DIR=${BIND10HOME}/src/bin/cfgmgr/plugins
export CONFIG_TESTDATA_PATH=${CTESTDATA}
export CONFIG_WR_TESTDATA_PATH=${CTESTDATA}
BDIR=${BIND10HOME}/src/lib/python/isc/log/tests
${BDIR}/check_output-win32.sh ${BDIR}/log_console.py ${BDIR}/console.out
if test $? -ne 0; then
    exit -1
fi
PYTESTS='log_test.py log_console.py'
for pytest in ${PYTESTS}
    do
        echo ${pytest}
        ${PYTHON} ${BIND10HOME}/src/lib/python/isc/log/tests/${pytest}
        if test $? -ne 0; then
            exit -1
        fi
    done

echo python net / Release
export PYTHONPATH=${CPYTHONPATH}
PYTESTS='addr_test.py parse_test.py'
for pytest in ${PYTESTS}
    do
        echo ${pytest}
        ${PYTHON} ${BIND10HOME}/src/lib/python/isc/net/tests/${pytest}
        if test $? -ne 0; then
            exit -1
        fi
    done

echo python notify / Release
export PYTHONPATH=${CPYTHONPATH}
export TESTDATASRCDIR=${BIND10HOME}/src/lib/python/isc/notify/tests/testdata/
PYTESTS='notify_out_test.py'
for pytest in ${PYTESTS}
    do
        echo ${pytest}
        ${PYTHON} ${BIND10HOME}/src/lib/python/isc/notify/tests/${pytest}
        if test $? -ne 0; then
            exit -1
        fi
    done

echo skipping python util cio / Release
export PYTHONPATH=${CPYTHONPATH}
export TESTDATAOBJDIR=${BIND10HOME}/src/lib/python/isc/util/cio/tests
PYTESTS='socketsession_test.py'
#for pytest in ${PYTESTS}
#    do
#        echo ${pytest}
#        ${PYTHON} ${BIND10HOME}/src/lib/python/isc/util/cio/tests/${pytest}
#        if test $? -ne 0; then
#            exit -1
#        fi
#    done

echo python util / Release
export PYTHONPATH=${CPYTHONPATH}
PYTESTS='process_test.py socketserver_mixin_test.py file_test_win32.py'
for pytest in ${PYTESTS}
    do
        echo ${pytest}
        ${PYTHON} ${BIND10HOME}/src/lib/python/isc/util/tests/${pytest}
        if test $? -ne 0; then
            exit -1
        fi
    done

echo python acl / Release
export PYTHONPATH=${CPYTHONPATH}
PYTESTS='acl_test.py dns_test.py'
for pytest in ${PYTESTS}
    do
        echo ${pytest}
        ${PYTHON} ${BIND10HOME}/src/lib/python/isc/acl/tests/${pytest}
        if test $? -ne 0; then
            exit -1
        fi
    done

echo skipping python bind10 / Release
export PYTHONPATH="${CPYTHONPATH};${BIND10HOME}/src/bin"
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/bin/bind10"
export B10_LOCKFILE_DIR_FROM_BUILD=${BIND10HOME}
#BIND10_MSGQ_SOCKET_FILE=
PYTESTS='sockcreator_test.py component_test.py socket_cache_test.py'
#for pytest in ${PYTESTS}
#    do
#        echo ${pytest}
#        ${PYTHON} ${BIND10HOME}/src/lib/python/isc/bind10/tests/${pytest}
#        if test $? -ne 0; then
#            exit -1
#        fi
#    done

echo python xfrin / Release
export PYTHONPATH=${CPYTHONPATH}
export B10_LOCKFILE_DIR_FROM_BUILD=${BIND10HOME}
PYTESTS='diff_tests.py'
for pytest in ${PYTESTS}
    do
        echo ${pytest}
        ${PYTHON} ${BIND10HOME}/src/lib/python/isc/xfrin/tests/${pytest}
        if test $? -ne 0; then
            exit -1
        fi
    done

echo python server-common / Release
export PYTHONPATH=${CPYTHONPATH}
export B10_LOCKFILE_DIR_FROM_BUILD=${BIND10HOME}
PYTESTS='tsig_keyring_test.py dns_tcp_test.py'
for pytest in ${PYTESTS}
    do
        echo ${pytest}
        ${PYTHON} \
            ${BIND10HOME}/src/lib/python/isc/server_common/tests/${pytest}
        if test $? -ne 0; then
            exit -1
        fi
    done

echo python ddns / Release
export PYTHONPATH=${CPYTHONPATH}
export TESTDATA_PATH=${BIND10HOME}/src/lib/testutils/testdata
export TESTDATA_WRITE_PATH=${BIND10HOME}/src/lib/python/isc/ddns/tests
PYTESTS='session_tests.py zone_config_tests.py'
for pytest in ${PYTESTS}
    do
        echo ${pytest}
        ${PYTHON} ${BIND10HOME}/src/lib/python/isc/ddns/tests/${pytest}
        if test $? -ne 0; then
            exit -1
        fi
    done

echo python sysinfo / Release
export PYTHONPATH=${CPYTHONPATH}
PYTESTS='sysinfo_test.py'
for pytest in ${PYTESTS}
    do
        echo ${pytest}
        ${PYTHON} ${BIND10HOME}/src/lib/python/isc/sysinfo/tests/${pytest}
        if test $? -ne 0; then
            exit -1
        fi
    done

