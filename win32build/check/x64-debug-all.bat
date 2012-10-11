@echo off
pushd %CD%
cd %BIND10HOME%
call .\win32build\check\exceptions-x64-debug.bat
popd
