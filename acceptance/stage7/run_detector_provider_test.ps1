$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$include = Join-Path $root 'firmware\include'
$out = Join-Path $env:TEMP 'pp-wp7-detector-provider-test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I $include (Join-Path $PSScriptRoot 'detector_provider_test.cpp') -o $out
if ($LASTEXITCODE -ne 0) { throw "WP7.1 detector-provider compile failed: $LASTEXITCODE" }
& $out
if ($LASTEXITCODE -ne 0) { throw "WP7.1 detector-provider acceptance failed: $LASTEXITCODE" }
Write-Host 'WP7.1 detector-provider contract PASS'
