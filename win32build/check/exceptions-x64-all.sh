pushd ${BIND10HOME} > /dev/null
./win32build/check/exceptions-x64-release.sh && \
./win32build/check/exceptions-x64-debug.sh
popd > /dev/null
