$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$include = Join-Path $root 'firmware\include'
$source = Join-Path $PSScriptRoot 'phase1_contract_test.cpp'
$out = Join-Path $PSScriptRoot 'phase1_contract_test.exe'
g++ -std=c++17 -Wall -Wextra -Werror -I $include $source -o $out
& $out
if ($LASTEXITCODE -ne 0) { throw "Stage 15 Phase 1 contract test failed." }
Write-Output 'Stage 15 Phase 1 contract PASS'
