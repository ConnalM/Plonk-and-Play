$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$include = Join-Path $root 'firmware\include'
$out = Join-Path $env:TEMP 'pp-s4-role-contract-test.exe'

& g++ -std=c++17 -Wall -Wextra -Werror -I $include `
  (Join-Path $PSScriptRoot 's4_role_contract_test.cpp') -o $out
if ($LASTEXITCODE -ne 0) { throw "S4 role-contract compile failed: $LASTEXITCODE" }
& $out
if ($LASTEXITCODE -ne 0) { throw "S4 role-contract behaviour failed: $LASTEXITCODE" }
Write-Host 'S4 shared role-contract host behaviour PASS'
