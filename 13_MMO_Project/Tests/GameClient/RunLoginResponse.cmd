@echo off
setlocal
set "ROOT=%~dp0..\.."
set "OUT=%ROOT%\Build\Tests\LoginResponse"
if not exist "%OUT%" mkdir "%OUT%" || exit /b 1
set "NET=%ROOT%\Build\Unreal\NativeNetwork\14.50.35717\NetworkCommonLib\NetworkCommonLib.lib"
cl /nologo /std:c++20 /EHsc /MD /utf-8 /DUNICODE /D_UNICODE /I"%ROOT%\Client\GameLogin" /I"%ROOT%\Common\NetworkCommon" /I"%ROOT%\Server\MHLib\includes" /I"%ROOT%\GameData\Generated\Common" /I"%ROOT%\Common\Packet\Game\Generated\Common" /Fo"%OUT%\LoginResponseTests.obj" /Fe"%OUT%\LoginResponseTests.exe" "%~dp0LoginResponseTests.cpp" "%NET%" || exit /b 1
"%OUT%\LoginResponseTests.exe"
exit /b %errorlevel%
