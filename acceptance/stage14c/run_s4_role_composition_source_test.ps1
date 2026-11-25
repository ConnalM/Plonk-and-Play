$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$out = Join-Path $env:TEMP 'pp-s4-role-composition-source-test.exe'
& g++ -std=c++17 -Wall -Wextra -Werror (Join-Path $PSScriptRoot 's4_role_composition_source_test.cpp') -o $out
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Push-Location $root
try { & $out; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE } }
finally { Pop-Location }
Write-Host 'S4b role-composition source boundary PASS'
