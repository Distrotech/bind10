@echo off
pushd %CD%
cd %BIND10HOME%
call .\win32build\check\exceptions-win32-release.bat && ^
call .\win32build\check\exceptions-win32-debug.bat
popd
