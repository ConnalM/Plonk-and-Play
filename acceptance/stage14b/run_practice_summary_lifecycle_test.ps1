$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$include = Join-Path $root 'firmware\include'
$source = Join-Path $PSScriptRoot 'practice_summary_lifecycle_test.cpp'
$out = Join-Path $PSScriptRoot 'practice_summary_lifecycle_test.exe'
g++ -std=c++17 -Wall -Wextra -Werror -I $include $source -o $out
& $out
$code = $LASTEXITCODE
Remove-Item -LiteralPath $out -Force -ErrorAction SilentlyContinue
if ($code -ne 0) { throw 'Practice Summary lifecycle test failed.' }
Write-Output 'Practice Summary lifecycle PASS'
