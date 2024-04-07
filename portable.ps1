$ErrorActionPreference = 'stop'
$PackageFilename = (Resolve-Path .\src\cascadia\CascadiaPackage\AppPackages\CascadiaPackage_0.0.1.0_x64_Test\CascadiaPackage_0.0.1.0_x64.msix).Path
$WindowsTerminalPackagePath = $PackageFilename
$outDirPath = "bin/x64/release/_appx"
if (Test-Path $outDirPath) {
    Remove-Item $outDirPath -Recurse -Force
}
New-Item -Type Directory $outDirPath -ErrorAction Ignore
$outDir = $outDirPath
$PackageFilename = Join-Path $outDir (Split-Path -Leaf "$($WindowsTerminalPackagePath)")
Copy-Item "$($WindowsTerminalPackagePath)" $PackageFilename
$XamlAppxPath = (Get-Item "src\cascadia\CascadiaPackage\AppPackages\*x64_Test\Dependencies\x64\Microsoft.UI.Xaml*.appx").FullName
$outDir = New-Item -Type Directory "$($outDir)/_unpackaged" -ErrorAction:Ignore
& .\build\scripts\New-UnpackagedTerminalDistribution.ps1 -PortableMode -TerminalAppX $($WindowsTerminalPackagePath) -XamlAppX $XamlAppxPath -Destination $outDir.FullName
$zipFile = Get-ChildItem $outDir *.zip
Expand-Archive $zipFile $outDir -Force
$termDirName = "terminal-0.0.1.0"
$expandedDir = Get-ChildItem $outDir $termDirName
$termDir = "~\utilities\$($termDirName)"
if (Test-Path $termDir) {`
    Remove-Item $termDir -Recurse -Force
}
Move-Item "$($expandedDir.FullName)" $termDir
$settingsDir = "$($termDir)\settings\"
New-Item $settingsDir -Type Directory

copy settings.json $settingsDir
