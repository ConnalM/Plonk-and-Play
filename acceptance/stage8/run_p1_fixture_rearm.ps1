$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$include = Join-Path $root 'firmware\include'
$source = Join-Path $PSScriptRoot 'p1_fixture_rearm_test.cpp'
$out = Join-Path $PSScriptRoot 'p1_fixture_rearm_test.exe'

$main = Get-Content (Join-Path $root 'firmware\src\main.cpp') -Raw
$releaseGuard = [regex]::Match($main, '(?s)#if defined\(PP_PHASE8B_P1_TEST\).*?const pp::Time fixtureNow=systemTime\(\);.*?input\.setSimulatedSourceB\(false\);simulatedBRelease=0;')
if (-not $releaseGuard.Success -or $releaseGuard.Value -notmatch 'PP_PHASE8B_P1_TEST') {
  throw 'P1 fixture release guard does not include PP_PHASE8B_P1_TEST.'
}
$fixturePath = [regex]::Match($main, '(?s)#if defined\(PP_PHASE8B_P1_TEST\).*?bool browserFixturePass\(uint8_t lane\).*?\n\}')
if (-not $fixturePath.Success -or $fixturePath.Value -notmatch 'setSimulatedSource\(true\)' -or $fixturePath.Value -notmatch 'simulatedARelease=systemTime\(\)\+150000') {
  throw 'P1 fixture assertion/release scheduling boundary is missing.'
}

g++ -std=c++17 -Wall -Wextra -Werror -I $include $source -o $out
if ($LASTEXITCODE -ne 0) { throw 'P1 detector re-arm test did not compile.' }
try {
  & $out
  if ($LASTEXITCODE -ne 0) { throw 'P1 detector re-arm executable failed.' }
} finally {
  Remove-Item -LiteralPath $out -Force -ErrorAction SilentlyContinue
}
Write-Output 'Stage 8B-P1 fixture assert/release/re-arm PASS'
