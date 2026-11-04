$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$include = Join-Path $root 'firmware\include'
$source = Join-Path $PSScriptRoot 'simulated_racing_journey_test.cpp'
$out = Join-Path $env:TEMP 'pp-simulated-racing-journey-test.exe'
g++ -std=c++17 -Wall -Wextra -Werror -I $include $source -o $out
if ($LASTEXITCODE -ne 0) { throw "Simulated racing journey compile failed: $LASTEXITCODE" }
& $out
$code = $LASTEXITCODE
Remove-Item -LiteralPath $out -Force -ErrorAction SilentlyContinue
if ($code -ne 0) { throw 'Simulated racing journey acceptance failed.' }
Write-Output 'Simulated racing journey PASS'
