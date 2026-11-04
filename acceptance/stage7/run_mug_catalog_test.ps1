$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$include = Join-Path $root 'firmware\include'
$out = Join-Path $env:TEMP 'pp-wp7-mug-catalog-test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I $include (Join-Path $PSScriptRoot 'mug_catalog_test.cpp') -o $out
if ($LASTEXITCODE -ne 0) { throw "WP7 saved MUG catalog compile failed: $LASTEXITCODE" }
& $out
if ($LASTEXITCODE -ne 0) { throw "WP7 saved MUG catalog acceptance failed: $LASTEXITCODE" }
Write-Host 'WP7 Saved MUG catalog foundation PASS'
