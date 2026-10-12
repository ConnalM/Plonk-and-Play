$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$include = Join-Path $root 'firmware\include'
$source = Join-Path $PSScriptRoot 'phase5_browser_trace_test.cpp'
$out = Join-Path $PSScriptRoot 'phase5_browser_trace_test.exe'
g++ -std=c++17 -Wall -Wextra -Werror -I $include -I $PSScriptRoot $source -o $out
& $out
$code = $LASTEXITCODE
Remove-Item -LiteralPath $out -Force -ErrorAction SilentlyContinue
if ($code -ne 0) { throw 'Stage 15 Phase 5 browser trace test failed.' }
$page = Get-Content (Join-Path $root 'firmware\browser\normal_browser.html') -Raw
foreach ($marker in @('diagnosticTraceId','/diag/browser','diagnosticAction')) {
  if (-not $page.Contains($marker)) { throw "normal Browser trace marker missing: $marker" }
}
if ($page.IndexOf("diagnosticAction(diagnosticOperationForPath(path),id,traceId)") -lt 0) { throw 'Browser action observation missing from request path.' }
$actionIndex = $page.IndexOf("diagnosticAction(diagnosticOperationForPath(path),id,traceId)")
$guardIndex = $page.IndexOf("if((path==='/request/start'", $actionIndex)
if ($guardIndex -lt 0 -or $guardIndex -lt $actionIndex) { throw 'Browser action must be observed before request suppression guard.' }
foreach ($marker in @('httpd_req_recv','diagnosticTraceId','observeBrowserAction')) {
  if (-not (Get-Content (Join-Path $root 'firmware\include\pp\browser_interface.h') -Raw).Contains($marker)) { throw "HTTP trace marker missing: $marker" }
}
Write-Output 'Stage 15 Phase 5 browser trace PASS'
& (Join-Path $PSScriptRoot 'run_phase4.ps1')
