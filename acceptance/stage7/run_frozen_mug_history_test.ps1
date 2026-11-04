$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$include = Join-Path $root 'firmware\include'
$out = Join-Path $env:TEMP 'pp-wp7-frozen-mug-history-test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I $include (Join-Path $PSScriptRoot 'frozen_mug_history_test.cpp') -o $out
if ($LASTEXITCODE -ne 0) { throw "WP7 frozen-history compile failed: $LASTEXITCODE" }
& $out
if ($LASTEXITCODE -ne 0) { throw "WP7 frozen-history acceptance failed: $LASTEXITCODE" }
Write-Host 'WP7 frozen racer history contract PASS'
