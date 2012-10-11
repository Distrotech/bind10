@echo off
setlocal
pushd %CD%
cd %BIND10HOME%
call .\win32build\env-win32-debug.bat
echo:
echo exceptions-tests / Win32.Debug
echo:
.\win32build\VS\exceptions-tests\Win32.Debug\run_unittests.exe
popd
endlocal
