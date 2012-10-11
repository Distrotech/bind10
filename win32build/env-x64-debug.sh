# Setting environment for BIND10 - x64.Debug

export B10_FROM_BUILD=${BIND10HOME}
parent=`cygpath ${BIND10HOME}/..`
b10home=`cygpath ${BIND10HOME}`
pydir=`cygpath ${PYTHONDIR}`
export PATH="${PATH}:${pydir}"
export PATH="${PATH}:${parent}gtest/x64.Debug"
export PATH="${PATH}:${parent}botan/x64.Debug"
export PATH="${PATH}:${parent}log4cplus/x64.Debug"
export PATH="${PATH}:${parent}sqlite3/x64.Debug"
export PATH="${PATH}:${b10home}/win32build/VS/x64.Debug"
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/win32build/VS/x64.Debug"
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/lib/python/isc/util"
export PYTHON=${pydir}/python_d.exe
export BIND10_PATH=${BIND10HOME}/src/bin/bind10
