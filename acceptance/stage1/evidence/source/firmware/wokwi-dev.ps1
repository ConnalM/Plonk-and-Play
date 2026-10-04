<#
Build the restartable local Wokwi development image.

Wokwi Restart loads the paths declared in wokwi.toml.  This script produces
those exact files; it does not upload firmware or change the normal build.
#>
$ErrorActionPreference = 'Stop'

& (Join-Path $PSScriptRoot 'build.ps1') -Environment 'wokwi-dev'
if ($LASTEXITCODE -ne 0) { throw "Wokwi development build failed (exit $LASTEXITCODE)." }

$image = Join-Path $PSScriptRoot '.pio\build\wokwi-dev\firmware.merged.bin'
$elf = Join-Path $PSScriptRoot '.pio\build\wokwi-dev\firmware.elf'
foreach ($path in @($image, $elf)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Expected Wokwi development artifact is missing: $path" }
}

Write-Host 'Wokwi development image is ready for Wokwi CLI or VS Code.'
Write-Host 'For the web custom-firmware project, upload this image once after creating a simulation; use serial r for later development resets.'
