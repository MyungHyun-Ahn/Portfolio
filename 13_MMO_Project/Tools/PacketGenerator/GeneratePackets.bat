@echo off
setlocal
if not defined PACKET_GENERATOR_PYTHON set "PACKET_GENERATOR_PYTHON=python"
if not "%~2"=="" goto usage
if "%~1"=="" goto chatting
if /i "%~1"=="Chatting" goto chatting
if /i "%~1"=="World" goto world
if /i "%~1"=="Game" goto game
goto usage

:chatting
set "PROTOCOL=Chatting"
set "PROTOCOL_NAMESPACE=Chatting"
set "CLIENT_CLASS=CChatNetClient"
set "CLIENT_DIR=Tests\Chatting\Client"
goto generate

:world
set "PROTOCOL=World"
set "PROTOCOL_NAMESPACE=WorldServer"
set "CLIENT_CLASS=WorldClient"
set "CLIENT_DIR=Tests\WorldServer"
goto generate

:game
set "PROTOCOL=Game"
set "PROTOCOL_NAMESPACE=Game"
set "CLIENT_CLASS=GameClient"
set "CLIENT_DIR=Client\GameLogin"

:generate
set "PACKET_ROOT=%~dp0..\..\Common\Packet\%PROTOCOL%"
"%PACKET_GENERATOR_PYTHON%" -B "%~dp0GeneratePackets.py" ^
  --namespace "%PROTOCOL_NAMESPACE%" --client-class "%CLIENT_CLASS%" ^
  --schema-dir "%PACKET_ROOT%\Schema" --common-out "%PACKET_ROOT%\Generated\Common" ^
  --client-out "%PACKET_ROOT%\Generated\Client" --server-out "%PACKET_ROOT%\Generated\Server" ^
  --client-members-out "%~dp0..\..\%CLIENT_DIR%\%CLIENT_CLASS%Members.inc"
exit /b %errorlevel%

:usage
>&2 echo Usage: GeneratePackets.bat [Chatting^|World^|Game]
exit /b 2
