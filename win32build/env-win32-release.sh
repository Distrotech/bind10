# Setting environment for BIND10 - Win32.Release

export B10_FROM_BUILD=${BIND10HOME}
parent=`cygpath ${BIND10HOME}/..`
b10home=`cygpath ${BIND10HOME}`
pydir=`cygpath ${PYTHONDIR}`
export PATH="${PATH}:${pydir}"
export PATH="${PATH}:${parent}gtest/Win32.Release"
export PATH="${PATH}:${parent}botan/Win32.Release"
export PATH="${PATH}:${parent}log4cplus/Win32.Release"
export PATH="${PATH}:${parent}sqlite3/Win32.Release"
export PATH="${PATH}:${b10home}/win32build/VS/Win32.Release"
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/win32build/VS/Win32.Release"
export PYTHONPATH="${PYTHONPATH};${BIND10HOME}/src/lib/python/isc/util"
export PYTHON=${pydir}/python.exe
export BIND10_PATH=${BIND10HOME}/src/bin/bind10
