. ${BIND10HOME}/win32build/env-debug.sh
echo
echo dns++-tests / Debug
echo
${BIND10HOME}/win32build/VS/dns++-tests/Debug/run_unittests.exe
if test $? -ne 0; then
    exit -1
fi
if [ ${PYTHON} == "None" ]; then
    echo skipping dns python / Debug
    exit 0
fi
echo dns python / Debug
export TESTDATA_PATH=${BIND10HOME}/src/lib/dns/tests/testdata
PYTESTS='edns_python_test.py
    message_python_test.py
    messagerenderer_python_test.py
    name_python_test.py
    nsec3hash_python_test.py
    question_python_test.py
    opcode_python_test.py
    rcode_python_test.py
    rdata_python_test.py
    rrclass_python_test.py
    rrset_python_test.py
    rrttl_python_test.py
    rrtype_python_test.py
    serial_python_test.py
    tsig_python_test.py
    tsig_rdata_python_test.py
    tsigerror_python_test.py
    tsigkey_python_test.py
    tsigrecord_python_test.py'
for pytest in ${PYTESTS}
    do
        echo ${pytest}
        ${PYTHON} ${BIND10HOME}/src/lib/dns/python/tests/${pytest}
        if test $? -ne 0; then
            exit -1
        fi
    done
