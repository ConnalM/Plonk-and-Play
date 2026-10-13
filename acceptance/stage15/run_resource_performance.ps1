$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $env:TEMP 'stage15_resource_performance.exe'

Write-Host 'Building Stage 15 resource/performance measurement test...'
g++ -std=c++17 -Wall -Wextra -Werror `
  -I (Join-Path $root 'firmware/include') `
  -I $PSScriptRoot `
  (Join-Path $PSScriptRoot 'resource_performance_test.cpp') `
  -o $exe

try {
  & $exe
  if ($LASTEXITCODE -ne 0) {
    throw "resource/performance test exited with code $LASTEXITCODE"
  }
}
finally {
  if (Test-Path $exe) { Remove-Item -Force $exe }
}

$browserHeader = Join-Path $root 'firmware/include/pp/browser_interface.h'
$browserText = Get-Content -Raw $browserHeader
$names = @{
  StateJsonCapacity = 'state'
  ResultsJsonCapacity = 'results'
  RecordsJsonCapacity = 'records'
  HistoryJsonCapacity = 'history'
}
foreach ($name in $names.Keys) {
  $match = [regex]::Match($browserText, "$name\s*=\s*(\d+)")
  if ($match.Success) { Write-Host ("CONFIGURED http_{0}_json_bytes={1}" -f $names[$name], $match.Groups[1].Value) }
}
$retrievalHeader = Join-Path $root 'firmware/include/pp/diagnostic_retrieval.h'
$retrievalText = Get-Content -Raw $retrievalHeader
$match = [regex]::Match($retrievalText, 'MaxPayloadBytes\s*=\s*(\d+)')
if ($match.Success) {
  $payload = [int]$match.Groups[1].Value
  Write-Host "CONFIGURED http_diag_records_payload_bytes=$payload"
  Write-Host ("CONFIGURED http_diag_records_json_bytes={0}" -f ($payload * 2 + 2048))
}
if ($browserText -notmatch '/diag/status' -or $browserText -notmatch '/diag/records') {
  throw 'diagnostic HTTP route markers missing from browser interface'
}
Write-Host 'STRUCTURAL diagnostic_http_routes=PASS'
