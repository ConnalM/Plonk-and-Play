$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $env:TEMP 'stage15_wp3_browser_observation.exe'
Write-Host 'Building WP3 Browser observation acceptance test...'
g++ -std=c++17 -Wall -Wextra -Werror `
  -I (Join-Path $root 'firmware/include') `
  (Join-Path $PSScriptRoot 'wp3_browser_observation_test.cpp') `
  -o $exe
try {
  & $exe
  if ($LASTEXITCODE -ne 0) {
    throw "WP3 Browser observation test exited with code $LASTEXITCODE"
  }
}
finally {
  if (Test-Path $exe) { Remove-Item -Force $exe }
}
$browser = Get-Content -Raw (Join-Path $root 'firmware/include/pp/browser_interface.h')
foreach ($marker in @('diagnostic::jsonTextField',
                      'diagnostic::browserStateObservationKind')) {
  if ($browser -notlike "*$marker*") { throw "Browser observation integration marker missing: $marker" }
}
Write-Host 'WP3 Browser observation parser/classification: PASS'
