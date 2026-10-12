$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$include = Join-Path $root 'firmware\include'
$source = Join-Path $PSScriptRoot 'phase3_backend_test.cpp'
$out = Join-Path $PSScriptRoot 'phase3_backend_test.exe'
g++ -std=c++17 -Wall -Wextra -Werror -I $include -I $PSScriptRoot $source -o $out
& $out
$code = $LASTEXITCODE
Remove-Item -LiteralPath $out -Force -ErrorAction SilentlyContinue
if ($code -ne 0) { throw "Stage 15 Phase 3 backend test failed." }
Write-Output 'Stage 15 Phase 3 backend PASS'
& (Join-Path $PSScriptRoot 'run_phase2.ps1')
