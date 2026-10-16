$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$browser = Get-Content -Raw (Join-Path $root 'firmware/browser/normal_browser.html')
$interface = Get-Content -Raw (Join-Path $root 'firmware/include/pp/browser_interface.h')
$production = Get-Content -Raw (Join-Path $root 'firmware/include/pp/diagnostic_production.h')
$main = Get-Content -Raw (Join-Path $root 'firmware/src/main.cpp')

foreach ($marker in @(
  'Settings','Advanced','diagnosticsPage','DOWNLOAD DIAGNOSTICS',
  '/diag/status','/diag/download','DEVELOPMENT LOGGING')) {
  if ($browser -notlike "*$marker*") { throw "Browser diagnostics marker missing: $marker" }
}
foreach ($marker in @(
  'diagnosticStartRoute','diagnosticStopRoute','diagnosticStatusRoute',
  'diagnosticRecordsRoute','diagnosticDownloadRoute','/diag/download',
  'application/x-ndjson','Content-Disposition')) {
  if ($interface -notlike "*$marker*") { throw "HTTP diagnostics marker missing: $marker" }
}
$export = Get-Content -Raw (Join-Path $root 'firmware/include/pp/diagnostic_export.h')
if ($export -notlike '*pp-diagnostics-v1*') { throw 'Diagnostic export schema marker missing' }
if ($interface -notmatch 'diagnosticStartRoute[\s\S]{0,700}diagnosticControlAllowed' -or
    $interface -notmatch 'diagnosticStopRoute[\s\S]{0,700}diagnosticControlAllowed') {
  throw 'Development start/stop lease boundary missing'
}
foreach ($route in @('diagnosticStatusRoute','diagnosticRecordsRoute','diagnosticDownloadRoute')) {
  $start = $interface.IndexOf("$route(")
  $next = $interface.IndexOf('static esp_err_t', $start + 1)
  if ($next -lt 0) { $next = $interface.Length }
  $body = if ($start -ge 0) { $interface.Substring($start, $next - $start) } else { '' }
  if (-not $body -or $body -match 'diagnosticControlAllowed') { throw "$route is not observation-only" }
}
foreach ($marker in @('DevelopmentLimitUs','developmentExpired_','developmentStatus','StorageHealth::RamOnly','StorageHealth::Degraded')) {
  if ($production -notlike "*$marker*") { throw "Production recorder marker missing: $marker" }
}
foreach ($marker in @('controllerTraceRecorder.beginNormal','browser.setDiagnosticTrace','controllerTrace.tick(systemTime())','PP_PHASE8B_REAL_SD')) {
  if ($main -notlike "*$marker*") { throw "Production integration marker missing: $marker" }
}
Write-Host 'WP3 Browser diagnostics/support contract: PASS'
