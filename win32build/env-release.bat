@rem Setting environment for BIND10 - Release

@echo off

pushd %CD%
cd %BIND10HOME%
set b10home=%CD%
cd ..
set b10parent=%CD%
popd

set B10_FROM_BUILD=%BIND10HOME%
set PATH=%PATH%;%PYTHONDIR%
set PATH=%PATH%;%b10parent%gtest\Release
set PATH=%PATH%;%b10parent%botan\Release
set PATH=%PATH%;%b10parent%log4cplus\Release
set PATH=%PATH%;%b10parent%sqlite3\Release
set PATH=%PATH%;%b10home%\win32build\VS\Release
set PYTHONPATH=%PYTHONPATH%;%b10home%\win32build\VS\Release
set PYTHONPATH=%PYTHONPATH%;%b10home%\src\lib\python\isc\util
set PYTHON=%PYTHONDIR%\python.exe
set BIND10_PATH=%BIND10HOME%/src/bin/bind10
