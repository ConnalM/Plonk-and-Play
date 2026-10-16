$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $PSScriptRoot 'wp3_production_test.exe'
$source = Join-Path $PSScriptRoot 'wp3_production_test.cpp'
$include = Join-Path $root '..\firmware\include'
g++ -std=c++17 -Wall -Wextra -Werror -I $include $source -o $exe
& $exe
if ($LASTEXITCODE -ne 0) { throw "WP3 production recorder acceptance failed: $LASTEXITCODE" }
Write-Host 'WP3 production recorder acceptance PASS'
