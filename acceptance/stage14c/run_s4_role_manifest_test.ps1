$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\\..')).Path
$include = Join-Path $root 'firmware\\include'
$out = Join-Path $env:TEMP 'pp-s4-role-manifest-test.exe'

# Host-only manifest contract. This compiles no ESP32 target and produces no
# firmware application image.
& g++ -std=c++17 -Wall -Wextra -Werror -I $include `
  (Join-Path $PSScriptRoot 's4_role_manifest_test.cpp') -o $out
if ($LASTEXITCODE -ne 0) { throw "S4b role-manifest compile failed: $LASTEXITCODE" }
& $out
if ($LASTEXITCODE -ne 0) { throw "S4b role-manifest contract failed: $LASTEXITCODE" }
Write-Host 'S4b role-manifest host contract PASS'
