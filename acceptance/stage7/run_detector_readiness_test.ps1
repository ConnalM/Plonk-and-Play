$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$include = Join-Path $root 'firmware\include'
$out = Join-Path $env:TEMP 'pp-wp7-detector-readiness-test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I $include (Join-Path $PSScriptRoot 'detector_readiness_test.cpp') -o $out
if ($LASTEXITCODE -ne 0) { throw "WP7.2 detector-readiness compile failed: $LASTEXITCODE" }
& $out
if ($LASTEXITCODE -ne 0) { throw "WP7.2 detector-readiness acceptance failed: $LASTEXITCODE" }
Write-Host 'WP7.2 detector readiness contract PASS'
