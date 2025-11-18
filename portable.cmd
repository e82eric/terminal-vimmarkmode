@echo off

git submodule update --init --recursive
call tools\razzle.cmd
msbuild OpenConsole.sln /p:WindowsTerminalOfficialBuild=true;WindowsTerminalBranding=Dev;PGOBuildMode=None /p:Configuration=Release /t:Terminal\CascadiaPackage
powershell -Command "Set-PSBreakpoint -Script .\portable.ps1 -Line 24"
if "%1"=="" (
    pwsh portable.ps1
) else (
    pwsh portable.ps1 %1
)
