$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $PSScriptRoot 'wp4a_diagnostic_test.exe'
$source = Join-Path $PSScriptRoot 'wp4a_diagnostic_test.cpp'
$include = Join-Path $root '..\firmware\include'
g++ -std=c++17 -Wall -Wextra -Werror -I $include $source -o $exe
& $exe
if ($LASTEXITCODE -ne 0) { throw "WP4A diagnostic durability contract failed: $LASTEXITCODE" }
Write-Host 'WP4A diagnostic durability contract PASS'
