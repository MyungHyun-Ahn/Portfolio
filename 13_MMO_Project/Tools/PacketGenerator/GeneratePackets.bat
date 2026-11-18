@echo off
setlocal
if not defined PACKET_GENERATOR_PYTHON set "PACKET_GENERATOR_PYTHON=python"
"%PACKET_GENERATOR_PYTHON%" -B "%~dp0GeneratePackets.py" ^
  --schema-dir "%~dp0..\..\Common\Packet\Schema" ^
  --common-out "%~dp0..\..\Common\Packet\Struct" ^
  --client-out "%~dp0..\..\Tests\Chatting\Client\Generated\PacketHandler" ^
  --server-out "%~dp0..\..\Tests\Chatting\Server\Generated\PacketHandler"
exit /b %errorlevel%
