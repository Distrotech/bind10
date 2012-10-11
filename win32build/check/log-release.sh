. ${BIND10HOME}/win32build/env-release.sh
echo
echo log-tests / Release
echo
${BIND10HOME}/win32build/VS/log-tests/Release/run_unittests.exe
if test $? -ne 0; then
    exit -1
fi
echo console / Release
(cd ${BIND10HOME}/win32build/VS/log-example/Release;
 ${BIND10HOME}/src/lib/log/tests/console_test.sh)
if test $? -ne 0; then
    exit -1
fi
echo destination / Release
(cd ${BIND10HOME}/win32build/VS/log-example/Release;
 ${BIND10HOME}/src/lib/log/tests/destination_test.sh)
if test $? -ne 0; then
    exit -1
fi
echo init logger / Release
(cd ${BIND10HOME}/win32build/VS/log-iltest/Release;
 ${BIND10HOME}/src/lib/log/tests/init_logger_test.sh)
if test $? -ne 0; then
    exit -1
fi
echo local file / Release
(cd ${BIND10HOME}/win32build/VS/log-example/Release;
 ${BIND10HOME}/src/lib/log/tests/local_file_test.sh)
if test $? -ne 0; then
    exit -1
fi
echo logger lock / Release
(cd ${BIND10HOME}/win32build/VS/log-lltest/Release;
 ${BIND10HOME}/src/lib/log/tests/logger_lock_test.sh)
if test $? -ne 0; then
    exit -1
fi
echo severity / Release
(cd ${BIND10HOME}/win32build/VS/log-example/Release;
 ${BIND10HOME}/src/lib/log/tests/local_file_test.sh)
if test $? -ne 0; then
    exit -1
fi
