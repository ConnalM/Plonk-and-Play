$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $env:TEMP 'stage15_control_lease_test.exe'

g++ -std=c++17 -Wall -Wextra -Werror `
  -I (Join-Path $root 'firmware/include') `
  (Join-Path $PSScriptRoot 'control_lease_test.cpp') -o $exe
try {
  & $exe
  if ($LASTEXITCODE -ne 0) { throw "control lease test exited with code $LASTEXITCODE" }
}
finally {
  if (Test-Path $exe) { Remove-Item -Force $exe }
}

$browser = Get-Content -Raw (Join-Path $root 'firmware/include/pp/browser_interface.h')
foreach ($marker in @(
  '/request/take-control',
  '/request/take-control/allow',
  '/request/take-control/keep',
  'controlConnected',
  'controlDisconnected',
  'diagnosticControlAllowed',
  '/diag/session/start',
  '/diag/session/stop'
)) {
  if ($browser -notlike "*$marker*") { throw "missing control-lease marker: $marker" }
}
if ($browser -notmatch 'diagnosticStatusRoute[\s\S]{0,1200}diagnosticControlAllowed' -or
    $browser -notmatch 'diagnosticRecordsRoute[\s\S]{0,1200}diagnosticControlAllowed') {
  throw 'diagnostic status/retrieval routes do not enforce the controlling lease'
}
$page = Get-Content -Raw (Join-Path $root 'firmware/browser/normal_browser.html')
foreach ($marker in @('takeControl()', 'allowControl', 'keepControl', '/request/take-control')) {
  if ($page -notlike "*$marker*") { throw "missing Browser takeover marker: $marker" }
}
Write-Host 'STAGE15 control-lease contract: PASS'
