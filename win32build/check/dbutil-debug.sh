. ${BIND10HOME}/win32build/env-debug.sh
echo
echo dbutil-tests / Debug
echo
export B10_LOCKFILE_DIR_FROM_BUILD=${BIND10HOME}
saved_VSCNF=${VSCNF}
export VSCNF=debug
cd ${BIND10HOME}/src/bin/dbutil/tests
sh ${BIND10HOME}/src/bin/dbutil/tests/dbutil_test.sh
status=$?
export VSCNF=${saved_VSCNF}
exit $status
