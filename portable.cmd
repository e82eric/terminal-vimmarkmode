git submodule update --init --recursive
call tools\razzle.cmd
msbuild OpenConsole.sln /p:WindowsTerminalOfficialBuild=true;WindowsTerminalBranding=Dev;PGOBuildMode=None /p:Configuration=Release /t:Terminal\CascadiaPackage
pwsh portable.ps1
