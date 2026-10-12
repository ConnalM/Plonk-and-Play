$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$include = Join-Path $root 'firmware\include'
$source = Join-Path $PSScriptRoot 'phase6_browser_render_trace_test.cpp'
$out = Join-Path $PSScriptRoot 'phase6_browser_render_trace_test.exe'
g++ -std=c++17 -Wall -Wextra -Werror -I $include -I $PSScriptRoot $source -o $out
& $out
$code = $LASTEXITCODE
Remove-Item -LiteralPath $out -Force -ErrorAction SilentlyContinue
if ($code -ne 0) { throw 'Stage 15 Phase 6 browser render trace test failed.' }
$page = Get-Content (Join-Path $root 'firmware\browser\normal_browser.html') -Raw
$embedded = Get-Content (Join-Path $root 'firmware\include\pp\normal_browser_page.inc') -Raw
foreach ($marker in @('/events','diagnosticObservation','state_fetch_started','state_received','state_applied','rendered','reconstruction','S=nextS','new EventSource')) {
  if (-not $page.Contains($marker)) { throw "Phase 6 Browser observation marker missing: $marker" }
}
foreach ($marker in @('diagnosticObservation','state_fetch_started','state_received','state_applied','rendered','reconstruction')) {
  if (-not $embedded.Contains($marker)) { throw "Generated Browser observation marker missing: $marker" }
}
if ($page.IndexOf("diagnosticObservation('notification'") -lt 0) { throw 'Browser notification receipt observation missing.' }
if ($page.IndexOf("diagnosticObservation('state_applied'") -lt 0) { throw 'Browser state application observation missing.' }
$browser = Get-Content (Join-Path $root 'firmware\include\pp\browser_interface.h') -Raw
foreach ($marker in @('/diag/browser-observation','browserObservationRoute','observeBrowserNotification','observeBrowserState')) {
  if (-not $browser.Contains($marker)) { throw "Phase 6 controller observation marker missing: $marker" }
}
Write-Output 'Stage 15 Phase 6 browser render trace PASS'
& (Join-Path $PSScriptRoot 'run_phase5.ps1')
