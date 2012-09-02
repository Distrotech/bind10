. ${BIND10HOME}/win32build/env-release.sh
echo
echo datasrc-tests / Release
echo
${BIND10HOME}/win32build/${VSVER}/datasrc-tests/Release/run_unittests.exe
if test $? -ne 0; then
    exit -1
fi
# memory
echo datasrc-factory-tests / Release
${BIND10HOME}/win32build/${VSVER}/datasrc-ftests/Release/run_unittests.exe
if test $? -ne 0; then
    exit -1
fi
