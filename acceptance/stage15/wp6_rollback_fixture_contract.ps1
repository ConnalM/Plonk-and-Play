param(
  [string]$Platformio = (Join-Path $PSScriptRoot '..\..\firmware\platformio.ini'),
  [string]$Source = (Join-Path $PSScriptRoot '..\..\firmware\src\main.cpp')
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Platformio)) { throw "PlatformIO configuration missing: $Platformio" }
if (-not (Test-Path -LiteralPath $Source)) { throw "Firmware source missing: $Source" }

$ini = Get-Content -Raw -LiteralPath $Platformio
$sections = [regex]::Matches($ini, '(?ms)^\[env:([^\]]+)\]\r?\n(.*?)(?=^\[env:|\z)')
$flagOwners = @()
foreach ($section in $sections) {
  if ($section.Groups[2].Value -match 'PP_WP6_ROLLBACK_ACCEPTANCE') {
    $flagOwners += $section.Groups[1].Value
  }
}

if ($flagOwners.Count -ne 1 -or $flagOwners[0] -ne 'stage8bp1s3sd-wp6-rollback-acceptance') {
  throw "Rollback fixture flag must be enabled only by the named acceptance environment; found: $($flagOwners -join ', ')"
}

$production = ($sections | Where-Object { $_.Groups[1].Value -eq 'stage8bp1s3sd' }).Groups[2].Value
if (-not $production -or $production -match 'PP_WP6_ROLLBACK_ACCEPTANCE') {
  throw 'Normal stage8bp1s3sd must not enable the rollback fixture.'
}

$sourceText = Get-Content -Raw -LiteralPath $Source
if ($sourceText -notmatch '(?s)#if\s+defined\(PP_WP6_ROLLBACK_ACCEPTANCE\).*?extern\s+"C"\s+bool\s+verifyOta\s*\(\s*\).*?return\s+false\s*;') {
  throw 'Acceptance-only verifyOta() false override is missing or not guarded.'
}

Write-Host 'WP6 rollback fixture contract PASS: only stage8bp1s3sd-wp6-rollback-acceptance enables verifyOta()=false.'
