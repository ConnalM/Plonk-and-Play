$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$include = Join-Path $root 'firmware\include'
$out = Join-Path $env:TEMP 'pp-wp7-racer-selection-test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I $include (Join-Path $PSScriptRoot 'racer_selection_test.cpp') -o $out
if ($LASTEXITCODE -ne 0) { throw "WP7.3 racer-selection compile failed: $LASTEXITCODE" }
& $out
if ($LASTEXITCODE -ne 0) { throw "WP7.3 racer-selection acceptance failed: $LASTEXITCODE" }
Write-Host 'WP7.3 Racer selection contract PASS'
