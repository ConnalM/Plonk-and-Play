$ErrorActionPreference = 'Stop'
$firmware = Split-Path -Parent $PSCommandPath
$pio = Get-Command pio -ErrorAction Stop
Push-Location $firmware
try {
  & $pio.Source run -e stage8atasterwokwi
  if ($LASTEXITCODE -ne 0) { throw "PlatformIO Taster Wokwi build failed (exit $LASTEXITCODE)." }
  $bin = Join-Path $firmware '.pio\build\stage8atasterwokwi\firmware.bin'
  if (-not (Test-Path -LiteralPath $bin)) { throw "Taster Wokwi firmware was not produced: $bin" }
  $hash = (Get-FileHash -LiteralPath $bin -Algorithm SHA256).Hash
  Write-Host "Taster Wokwi firmware: $bin"
  Write-Host "Size: $((Get-Item -LiteralPath $bin).Length) bytes"
  Write-Host "SHA-256: $hash"
} finally {
  Pop-Location
}
