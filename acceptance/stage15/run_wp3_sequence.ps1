$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $PSScriptRoot 'wp3_sequence_test.exe'
$source = Join-Path $PSScriptRoot 'wp3_sequence_test.cpp'
$include = Join-Path $root '..\firmware\include'

g++ -std=c++17 -Wall -Wextra -Werror -I $include $source -o $exe
try {
  & $exe
  if ($LASTEXITCODE -ne 0) { throw "WP3 sequence acceptance failed: $LASTEXITCODE" }
}
finally {
  if (Test-Path $exe) { Remove-Item -Force $exe }
}
Write-Host 'WP3 durable diagnostic sequence acceptance PASS'
