$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $PSScriptRoot 'taster_surface_test.exe'
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation `
  (Join-Path $PSScriptRoot '..\stage8a_taster_surface_test.cpp') `
  -I (Join-Path $root '..\firmware\include') `
  -o $out
& $out
if ($LASTEXITCODE -ne 0) { throw "Taster surface test failed" }
