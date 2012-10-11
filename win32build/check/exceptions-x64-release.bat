@echo off
setlocal
pushd %CD%
cd %BIND10HOME%
call .\win32build\env-x64-release.bat
echo:
echo exceptions-tests / x64.Release
echo:
.\win32build\VS\exceptions-tests\x64.Release\run_unittests.exe
popd
endlocal
