@echo off
setlocal
pushd %CD%
cd %BIND10HOME%
call .\win32build\env-win32-release.bat
echo:
echo exceptions-tests / Win32.Release
echo:
.\win32build\VS\exceptions-tests\Win32.Release\run_unittests.exe
popd
endlocal
