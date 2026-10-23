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
$constructorStart = $browser.IndexOf('BrowserInterface(Bus&')
$constructorEnd = $browser.IndexOf('uint64_t controllerEpoch()', $constructorStart)
$constructor = if ($constructorStart -ge 0 -and $constructorEnd -gt $constructorStart) { $browser.Substring($constructorStart, $constructorEnd - $constructorStart) } else { '' }
if ($constructor -match 'loadMaster') {
  throw 'BrowserInterface constructor must not load persisted authority before the store is ready'
}
if ($browser -notmatch 'bool restorePersistedAuthority\(\)[\s\S]*?authority_->loadMaster\(persisted\)') {
  throw 'persisted authority restoration is not present in the post-begin restoration path'
}
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
if ($browser -notmatch 'diagnosticStartRoute[\s\S]{0,700}diagnosticControlAllowed' -or
    $browser -notmatch 'diagnosticStopRoute[\s\S]{0,700}diagnosticControlAllowed') {
  throw 'diagnostic start/stop routes do not enforce the controlling lease'
}
foreach ($route in @('diagnosticStatusRoute','diagnosticRecordsRoute','diagnosticDownloadRoute')) {
  $start = $browser.IndexOf("$route(")
  $next = $browser.IndexOf('static esp_err_t', $start + 1)
  if ($next -lt 0) { $next = $browser.Length }
  $body = if ($start -ge 0) { $browser.Substring($start, $next - $start) } else { '' }
  if (-not $body -or $body -match 'diagnosticControlAllowed') {
    throw "$route must remain observation-only for connected Browsers"
  }
}
$page = Get-Content -Raw (Join-Path $root 'firmware/browser/normal_browser.html')
foreach ($marker in @('takeControl()', 'allowControl', 'keepControl', '/request/take-control')) {
  if ($page -notlike "*$marker*") { throw "missing Browser takeover marker: $marker" }
}
Write-Host 'STAGE15 control-lease contract: PASS'
