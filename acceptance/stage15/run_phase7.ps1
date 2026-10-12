$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$include = Join-Path $root 'firmware\include'
$source = Join-Path $PSScriptRoot 'phase7_retrieval_test.cpp'
$out = Join-Path $PSScriptRoot 'phase7_retrieval_test.exe'
g++ -std=c++17 -Wall -Wextra -Werror -I $include -I $PSScriptRoot $source -o $out
& $out
$code = $LASTEXITCODE
Remove-Item -LiteralPath $out -Force -ErrorAction SilentlyContinue
if ($code -ne 0) { throw 'Stage 15 Phase 7 retrieval/resource test failed.' }

$browser = Get-Content (Join-Path $root 'firmware\include\pp\browser_interface.h') -Raw
foreach ($marker in @('/diag/status','diagnosticStatusRoute','/diag/records','diagnosticRecordsRoute','DiagnosticRetrieval::next')) {
  if (-not $browser.Contains($marker)) { throw "Phase 7 HTTP diagnostic marker missing: $marker" }
}
$trace = Get-Content (Join-Path $root 'firmware\include\pp\diagnostic_trace.h') -Raw
if (-not $trace.Contains('storageStatus')) { throw 'Phase 7 recorder storage-status marker missing' }
$retrieval = Get-Content (Join-Path $root 'firmware\include\pp\diagnostic_retrieval.h') -Raw
foreach ($marker in @('RetrievalChunk','MaxPayloadBytes','evidenceIncomplete','readFailed','nextCursor')) {
  if (-not $retrieval.Contains($marker)) { throw "Phase 7 retrieval marker missing: $marker" }
}
Write-Output 'Stage 15 Phase 7 retrieval/resource PASS'
& (Join-Path $PSScriptRoot 'run_phase6.ps1')
