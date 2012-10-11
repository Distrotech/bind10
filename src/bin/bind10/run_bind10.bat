@echo off

rem Copyright (C) 2012  Internet Systems Consortium.
rem
rem Permission to use, copy, modify, and distribute this software for any
rem purpose with or without fee is hereby granted, provided that the above
rem copyright notice and this permission notice appear in all copies.
rem
rem THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SYSTEMS CONSORTIUM
rem DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
rem IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
rem INTERNET SYSTEMS CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
rem INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
rem FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
rem NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
rem WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

pushd %CD%
cd %BIND10HOME%
set b10home=%CD%
popd

if "%VSCNF%"=="" set VSCNF=release

call %b10home%\win32build\env-%VSCNF%.bat

set BIND10_PATH=%b10home%\src\bin\bind10

set PATH=%b10home%\src\bin\sockcreator;%PATH%
set PATH=%b10home%\src\bin\dhcp6;%PATH%
set PATH=%b10home%\src\bin\ddns;%PATH%
set PATH=%b10home%\src\bin\zonemgr;%PATH%
set PATH=%b10home%\src\bin\xfrout;%PATH%
set PATH=%b10home%\src\bin\xfrin;%PATH%
set PATH=%b10home%\src\bin\stats;%PATH%
set PATH=%b10home%\src\bin\cmdctl;%PATH%
set PATH=%b10home%\src\bin\cfgmgr;%PATH%
set PATH=%b10home%\src\bin\resolver;%PATH%
set PATH=%b10home%\src\bin\auth;%PATH%
set PATH=%b10home%\src\bin\msgq;%PATH%

set PYTHONPATH=%PYTHONPATH%;%b10home%\src\lib\python\isc\log_messages
set PYTHONPATH=%PYTHONPATH%;%b10home%\src\lib\python
set PYTHONPATH=%PYTHONPATH%;%b10home%\src\lib\python\isc\config

set B10_FROM_SOURCE=%BIND10HOME%
rem TODO: We need to do this feature based (ie. no general from_source)
rem But right now we need a second one because some spec files are
rem generated and hence end up under builddir
set B10_FROM_BUILD=%BIND10HOME%

set BIND10_MSGQ_SOCKET_FILE=%BIND10HOME%/msgq_socket

%PYTHON% -O %BIND10_PATH%\bind10_src.py %*
