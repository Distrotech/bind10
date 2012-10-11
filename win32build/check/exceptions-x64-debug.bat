@echo off
setlocal
pushd %CD%
cd %BIND10HOME%
call .\win32build\env-x64-debug.bat
echo:
echo exceptions-tests / x64.Debug
echo:
.\win32build\VS\exceptions-tests\x64.Debug\run_unittests.exe
popd
endlocal
