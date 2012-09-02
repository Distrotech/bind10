. ${BIND10HOME}/win32build/env-release.sh
echo
echo dbutil-tests / Release
echo
export B10_LOCKFILE_DIR_FROM_BUILD=${BIND10HOME}
saved_VSCNF=${VSCNF}
export VSCNF=release
cd ${BIND10HOME}/src/bin/dbutil/tests
sh ${BIND10HOME}/src/bin/dbutil/tests/dbutil_test.sh
status=$?
export VSCNF=${saved_VSCNF}
exit $status
