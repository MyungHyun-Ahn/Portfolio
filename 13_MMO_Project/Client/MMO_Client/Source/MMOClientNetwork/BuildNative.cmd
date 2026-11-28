@echo off
setlocal
set "UEPROJECT=%~dp0..\.."
set "ROOT=%UEPROJECT%\..\.."
set "VCVER=14.50.35717"
set "SDK=10.0.22621.0"
set "MSBUILD="
pushd "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer" || exit /b 1
for /f "usebackq delims=" %%I in (`vswhere.exe -version "[18.0,19.0)" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find MSBuild\Current\Bin\amd64\MSBuild.exe`) do set "MSBUILD=%%I"
popd
if not defined MSBUILD exit /b 1
call :build NetworkCommonLib Common\NetworkCommon || exit /b 1
call :build ClientNetworkLib Client\ClientNetworkLib || exit /b 1
call :build UEChatLogin Client\UEChatLogin || exit /b 1
call :build GameLogin Client\GameLogin || exit /b 1
exit /b 0
:build
set "OUT=%ROOT%\Build\Unreal\NativeNetwork\%VCVER%"
"%MSBUILD%" "%ROOT%\%2\%1.vcxproj" /nologo /v:minimal /t:Build /p:BuildProjectReferences=false /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /p:VCToolsVersion=%VCVER% /p:WindowsTargetPlatformVersion=%SDK% "/p:NativeOutputRoot=%OUT%" "/p:ForceImportBeforeCppTargets=%~dp0NativeOutputPaths.props"
exit /b %ERRORLEVEL%
