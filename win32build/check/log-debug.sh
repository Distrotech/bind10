. ${BIND10HOME}/win32build/env-debug.sh
echo
echo log-tests / Debug
echo
${BIND10HOME}/win32build/VS/log-tests/Debug/run_unittests.exe
if test $? -ne 0; then
    exit -1
fi
echo console / Debug
(cd ${BIND10HOME}/win32build/VS/log-example/Debug;
 ${BIND10HOME}/src/lib/log/tests/console_test.sh)
if test $? -ne 0; then
    exit -1
fi
echo destination / Debug
(cd ${BIND10HOME}/win32build/VS/log-example/Debug;
 ${BIND10HOME}/src/lib/log/tests/destination_test.sh)
if test $? -ne 0; then
    exit -1
fi
echo init logger / Debug
(cd ${BIND10HOME}/win32build/VS/log-iltest/Debug;
 ${BIND10HOME}/src/lib/log/tests/init_logger_test.sh)
if test $? -ne 0; then
    exit -1
fi
echo local file / Debug
(cd ${BIND10HOME}/win32build/VS/log-example/Debug;
 ${BIND10HOME}/src/lib/log/tests/local_file_test.sh)
if test $? -ne 0; then
    exit -1
fi
echo logger lock / Debug
(cd ${BIND10HOME}/win32build/VS/log-lltest/Debug;
 ${BIND10HOME}/src/lib/log/tests/logger_lock_test.sh)
if test $? -ne 0; then
    exit -1
fi
echo severity / Debug
(cd ${BIND10HOME}/win32build/VS/log-example/Debug;
 ${BIND10HOME}/src/lib/log/tests/local_file_test.sh)
if test $? -ne 0; then
    exit -1
fi
