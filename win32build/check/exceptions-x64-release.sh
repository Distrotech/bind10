pushd ${BIND10HOME} > /dev/null
. ./win32build/env-x64-release.sh
echo
echo exceptions-tests / x64.Release
echo
./win32build/VS/exceptions-tests/x64.Release/run_unittests.exe
popd > /dev/null
