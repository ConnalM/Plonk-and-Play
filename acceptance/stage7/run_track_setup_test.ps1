$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$include = Join-Path $root 'firmware\include'
$out = Join-Path $env:TEMP 'pp-wp7-track-setup-test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I $include (Join-Path $PSScriptRoot 'track_setup_test.cpp') -o $out
if ($LASTEXITCODE -ne 0) { throw "WP7.3 Track Setup compile failed: $LASTEXITCODE" }
& $out
if ($LASTEXITCODE -ne 0) { throw "WP7.3 Track Setup acceptance failed: $LASTEXITCODE" }
Write-Host 'WP7.3 Track Setup contract PASS'
