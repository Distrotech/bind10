. ${BIND10HOME}/win32build/env-debug.sh
echo
echo loadzone-tests / Debug
echo
if [ ${PYTHON} == "None" ]; then
    echo skipping loadzone python / Debug
    exit 0
fi
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/lib/python/isc/log_messages"
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/lib/python"
saved_VSCNF=${VSCNF}
export VSCNF=debug
cd ${BIND10HOME}/src/bin/loadzone/tests/correct
sh ${BIND10HOME}/src/bin/loadzone/tests/correct/correct_test.sh
if test $? -ne 0; then
    export VSCNF=${saved_VSCNF}
    exit -1
fi
cd ${BIND10HOME}/src/bin/loadzone/tests/error
sh ${BIND10HOME}/src/bin/loadzone/tests/error/error_test.sh
if test $? -ne 0; then
    export VSCNF=${saved_VSCNF}
    exit -1
fi
export VSCNF=${saved_VSCNF}
