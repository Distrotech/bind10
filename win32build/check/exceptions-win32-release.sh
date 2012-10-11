pushd ${BIND10HOME} > /dev/null
. ./win32build/env-win32-release.sh
echo
echo exceptions-tests / Win32.Release
echo
./win32build/VS/exceptions-tests/Win32.Release/run_unittests.exe
popd > /dev/null
