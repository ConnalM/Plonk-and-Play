$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$include = Join-Path $root 'firmware\include'
$out = Join-Path $env:TEMP 'pp-taster-remote-authority-test.exe'

& g++ -std=c++17 -Wall -Wextra -Werror -I $include `
  (Join-Path $PSScriptRoot 'taster_remote_authority_test.cpp') -o $out
if ($LASTEXITCODE -ne 0) { throw "Remote Taster authority compile failed: $LASTEXITCODE" }
& $out
if ($LASTEXITCODE -ne 0) { throw "Remote Taster authority behaviour failed: $LASTEXITCODE" }
Write-Host 'Remote Taster authority host contract PASS'
