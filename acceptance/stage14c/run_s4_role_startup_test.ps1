$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$include = Join-Path $root 'firmware\include'
$out = Join-Path $env:TEMP 'pp-s4-role-startup-test.exe'

& g++ -std=c++17 -Wall -Wextra -Werror -I $include `
  (Join-Path $PSScriptRoot 's4_role_startup_test.cpp') `
  (Join-Path $root 'firmware\role_boundaries\s4_core_entrypoint_boundary.cpp') `
  (Join-Path $root 'firmware\role_boundaries\s4_taster_entrypoint_boundary.cpp') -o $out
if ($LASTEXITCODE -ne 0) { throw "S4 role-startup compile failed: $LASTEXITCODE" }
& $out
if ($LASTEXITCODE -ne 0) { throw "S4 role-startup contract failed: $LASTEXITCODE" }
& g++ -std=c++17 -Wall -Wextra -Werror -I $include -DPP_S4_PRODUCTION_CORE `
  -c (Join-Path $root 'firmware\role_boundaries\s4_core_entrypoint_boundary.cpp') `
  -o (Join-Path $env:TEMP 'pp-s4-core-entrypoint.o')
if ($LASTEXITCODE -ne 0) { throw "S4 Core entry-point boundary compile failed: $LASTEXITCODE" }
& g++ -std=c++17 -Wall -Wextra -Werror -I $include -DPP_S4_PRODUCTION_TASTER `
  -c (Join-Path $root 'firmware\role_boundaries\s4_taster_entrypoint_boundary.cpp') `
  -o (Join-Path $env:TEMP 'pp-s4-taster-entrypoint.o')
if ($LASTEXITCODE -ne 0) { throw "S4 Taster entry-point boundary compile failed: $LASTEXITCODE" }
$coreWrong = & g++ -std=c++17 -Wall -Wextra -Werror -I $include -DPP_S4_PRODUCTION_TASTER `
  -c (Join-Path $root 'firmware\role_boundaries\s4_core_entrypoint_boundary.cpp') `
  -o (Join-Path $env:TEMP 'pp-s4-core-wrong-role.o') 2>&1
$coreWrongText = $coreWrong | Out-String
if ($LASTEXITCODE -eq 0 -or $coreWrongText -notmatch 'Core boundary cannot be selected') {
  throw 'S4 Core boundary did not reject the Taster production role'
}
$tasterWrong = & g++ -std=c++17 -Wall -Wextra -Werror -I $include -DPP_S4_PRODUCTION_CORE `
  -c (Join-Path $root 'firmware\role_boundaries\s4_taster_entrypoint_boundary.cpp') `
  -o (Join-Path $env:TEMP 'pp-s4-taster-wrong-role.o') 2>&1
$tasterWrongText = $tasterWrong | Out-String
if ($LASTEXITCODE -eq 0 -or $tasterWrongText -notmatch 'Taster boundary cannot be selected') {
  throw 'S4 Taster boundary did not reject the Core production role'
}
Write-Host 'S4 role startup ownership host contract PASS'
