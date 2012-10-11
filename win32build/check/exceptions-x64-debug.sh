pushd ${BIND10HOME} > /dev/null
. ./win32build/env-x64-debug.sh
echo
echo exceptions-tests / x64.Debug
echo
./win32build/VS/exceptions-tests/x64.Debug/run_unittests.exe
popd > /dev/null
