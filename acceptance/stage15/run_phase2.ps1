$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$include = Join-Path $root 'firmware\include'
$source = Join-Path $PSScriptRoot 'phase2_memory_test.cpp'
$out = Join-Path $PSScriptRoot 'phase2_memory_test.exe'
g++ -std=c++17 -Wall -Wextra -Werror -I $include $source -o $out
& $out
if ($LASTEXITCODE -ne 0) { throw "Stage 15 Phase 2 memory test failed." }
Write-Output 'Stage 15 Phase 2 memory/session PASS'
& (Join-Path $PSScriptRoot 'run_phase1.ps1')
