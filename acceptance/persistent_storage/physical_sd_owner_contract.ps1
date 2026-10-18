$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$owner = Get-Content (Join-Path $root 'firmware\include\pp\physical_sd_owner.h') -Raw
$history = Get-Content (Join-Path $root 'firmware\include\pp\sd_store_arduino.h') -Raw
$diagnostic = Get-Content (Join-Path $root 'firmware\include\pp\diagnostic_sd.h') -Raw
$main = Get-Content (Join-Path $root 'firmware\src\main.cpp') -Raw
$psf = Get-Content (Join-Path $root 'docs\PERSISTENT_STORAGE_FOUNDATION_DESIGN.md') -Raw
$psfAcceptance = Get-Content (Join-Path $root 'docs\ACCEPTANCE_TESTS_PERSISTENT_STORAGE_FOUNDATION.md') -Raw
$stage15 = Get-Content (Join-Path $root 'docs\ACCEPTANCE_TESTS_STAGE_15_LOGGING_DIAGNOSTICS_FOUNDATION.md') -Raw

if ($owner -notmatch 'SD\.begin\(' -or $owner -notmatch 'SPIClass\s+spi_') { throw 'Physical owner does not contain the sole physical SD/SPI initialization' }
if ($history -match 'SD\.begin\(' -or $history -match 'SPIClass\s+spi_') { throw 'History/detail consumer still owns physical SD initialization' }
if ($diagnostic -match 'SD\.begin\(' -or $diagnostic -match 'SPIClass\s+spi_') { throw 'Diagnostic consumer still owns physical SD initialization' }
if ($history -match '(?m)^\s*SD\.' -or $diagnostic -match '(?m)^\s*SD\.') { throw 'Logical consumer bypasses the physical owner boundary' }
if ($main -notmatch 'PhysicalSdOwner\s+physicalSdOwner' -or
    $main -notmatch 'Esp32SpiSdStore\s+sdPersistence\(physicalSdOwner\)' -or
    $main -notmatch 'Esp32SpiSdDiagnosticStorage\s+controllerTraceStorage\(physicalSdOwner\)' -or
    $main -notmatch 'physicalSdOwner\.begin\(\)' -or
    $main -notmatch 'servicePhysicalSdRecovery\(\)') { throw 'P&P main path does not wire both logical consumers through one owner' }
if ($owner -notmatch 'SessionLifecycle::Ready' -or $owner -notmatch 'SessionLifecycle::Finished' -or
    $owner -notmatch 'SessionLifecycle::Starting' -or $owner -notmatch 'SessionLifecycle::Racing' -or
    $owner -notmatch 'SessionLifecycle::Paused' -or $owner -notmatch 'SessionLifecycle::Restarting') { throw 'Lifecycle recovery gate is incomplete' }
if ($owner -notmatch 'xSemaphoreTakeRecursive\([^,]+,\s*0\)') { throw 'Storage arbitration is not bounded/non-blocking' }
foreach ($id in @('PSF.31','PSF.32','PSF.33','PSF.34','PSF.35','PSF.36','PSF.37')) {
  if (($psf + $psfAcceptance) -notmatch [regex]::Escape($id)) { throw "Missing $id design/acceptance wording" }
}
foreach ($id in @('PS15.88','PS15.89','PS15.90','PS15.91','PS15.92')) {
  if ($stage15 -notmatch [regex]::Escape($id)) { throw "Missing $id design/acceptance wording" }
}
$productionHeaders = Get-ChildItem (Join-Path $root 'firmware\include\pp') -Filter '*.h' -File |
  ForEach-Object { Get-Content $_.FullName -Raw }
$productionBeginCount = (($productionHeaders -join "`n") + $main | Select-String -Pattern 'SD\.begin\(' -AllMatches).Matches.Count
if ($productionBeginCount -ne 1) { throw "Expected one production SD.begin call, found $productionBeginCount" }
Write-Host 'PSF.31-PSF.37 PASS single-owner mount/arbitration/lifecycle boundary'
Write-Host 'PS15.88-PS15.92 PASS shared-owner diagnostics/recovery/failure-isolation boundary'
