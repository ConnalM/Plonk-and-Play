$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$include = Join-Path $root 'firmware\include'
$source = Join-Path $PSScriptRoot 'p1_noticeboard_streamed_detail_test.cpp'
$out = Join-Path $PSScriptRoot 'p1_noticeboard_streamed_detail_test.exe'
g++ -std=c++17 -Wall -Wextra -Werror -I $include $source -o $out
if ($LASTEXITCODE -ne 0) { throw 'P1 streamed-detail Noticeboard test did not compile.' }
try {
  & $out
  if ($LASTEXITCODE -ne 0) { throw 'P1 streamed-detail Noticeboard test failed.' }
} finally {
  Remove-Item -LiteralPath $out -Force -ErrorAction SilentlyContinue
}
Write-Output 'Stage 8B-P1 streamed-detail Noticeboard reconstruction PASS'
