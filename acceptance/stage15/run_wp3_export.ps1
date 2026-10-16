$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $env:TEMP 'stage15_wp3_export.exe'

Write-Host 'Building WP3 diagnostic export acceptance test...'
g++ -std=c++17 -Wall -Wextra -Werror `
  -I (Join-Path $root 'firmware/include') `
  (Join-Path $PSScriptRoot 'wp3_export_test.cpp') `
  -o $exe

try {
  & $exe
  if ($LASTEXITCODE -ne 0) {
    throw "WP3 export acceptance test exited with code $LASTEXITCODE"
  }
}
finally {
  if (Test-Path $exe) { Remove-Item -Force $exe }
}

$interface = Get-Content -Raw (Join-Path $root 'firmware/include/pp/browser_interface.h')
foreach ($marker in @(
  'DiagnosticExport::write',
  'P-and-P-Diagnostics.ndjson',
  'diagnosticExportCapability',
  'diagnosticBackendName')) {
  if ($interface -notlike "*$marker*") { throw "Download route marker missing: $marker" }
}
Write-Host 'WP3 /diag/download encoder and route integration: PASS'
