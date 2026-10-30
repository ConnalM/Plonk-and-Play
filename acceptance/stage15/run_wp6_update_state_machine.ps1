$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $PSScriptRoot 'wp6_update_state_machine_test.exe'
$src = Join-Path $PSScriptRoot 'wp6_update_state_machine_test.cpp'
$include = Join-Path $root '..\firmware\include'
& g++ -std=c++17 -Wall -Wextra -Werror -I $include $src -o $out
if ($LASTEXITCODE -ne 0) { throw 'WP6 update state-machine compile failed' }
& $out
if ($LASTEXITCODE -ne 0) { throw 'WP6 update state-machine test failed' }
