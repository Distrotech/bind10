. ${BIND10HOME}/win32build/env-debug.sh
echo
echo datasrc-tests / Debug
echo
${BIND10HOME}/win32build/VS/datasrc-tests/Debug/run_unittests.exe
if test $? -ne 0; then
    exit -1
fi
# memory
echo datasrc-factory-tests / Debug
${BIND10HOME}/win32build/VS/datasrc-ftests/Debug/run_unittests.exe
if test $? -ne 0; then
    exit -1
fi
