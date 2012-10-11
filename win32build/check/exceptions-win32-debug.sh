pushd ${BIND10HOME} > /dev/null
. ./win32build/env-win32-debug.sh
echo
echo exceptions-tests / Win32.Debug
echo
./win32build/VS/exceptions-tests/Win32.Debug/run_unittests.exe
popd > /dev/null
