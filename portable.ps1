param($deployPath = "$($home)\utilities")

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
$termDir = "$($deployPath)\$($termDirName)"
$settingsDir = "$($termDir)\settings\"

$settingFileToDepoy = ConvertFrom-Json (gc settings.json | Out-String)
$existingSettingsFile = "$($settingsDir)\settings.json"
if (Test-Path $termDir) {`
  if (Test-Path $existingSettingsFile) {
    Write-Host "Found existing settings file attemption to merge send input actions. Path: $($existingSettingsFile)"
    $si = ConvertFrom-Json (gc $existingSettingsFile | Out-String)
    $jti = $settingFileToDepoy.actions | % { $_.command.input }
    $si.actions | % {
      if ($jti -Contains $_.command.input) {
        Write-Host "Found: $($_.command.input)"
      } else {
        Write-Host "Not Found: $($_.command.input)"
        $settingFileToDepoy.actions += $_
      }
    }
  } else {
    Write-Host "Existing settings file not found skipping send input merge. Path: $($existingSettingsFile)"
  }

  Write-Host "Found existing terminal install removing it"
  Remove-Item $termDir -Recurse -Force
}
Move-Item "$($expandedDir.FullName)" $termDir
New-Item $settingsDir -Type Directory

$settingFileToDepoy | ConvertTo-Json -Depth 32 | Set-Content -Path "$($settingsDir)\settings.json"
