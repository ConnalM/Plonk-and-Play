$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$owner = Get-Content (Join-Path $root 'firmware\include\pp\physical_sd_owner.h') -Raw
$browser = Get-Content (Join-Path $root 'firmware\include\pp\browser_interface.h') -Raw
$main = Get-Content (Join-Path $root 'firmware\src\main.cpp') -Raw
$psf = Get-Content (Join-Path $root 'docs\PERSISTENT_STORAGE_FOUNDATION_DESIGN.md') -Raw
$acceptance = Get-Content (Join-Path $root 'docs\ACCEPTANCE_TESTS_PERSISTENT_STORAGE_FOUNDATION.md') -Raw
$stage15 = Get-Content (Join-Path $root 'docs\ACCEPTANCE_TESTS_STAGE_15_LOGGING_DIAGNOSTICS_FOUNDATION.md') -Raw

foreach ($marker in @(
  'RecoveryRequestResult', 'Accepted', 'AlreadyPending', 'AlreadyHealthy',
  'LifecycleRejected', 'requestRecovery()', 'recoveryPending_',
  'consumeRecoveryRequest()', 'markRecoveryFailure()')) {
  if ($owner -notmatch [regex]::Escape($marker)) { throw "owner recovery-request marker missing: $marker" }
}
if ($owner -notmatch 'requestRecovery\(\)[\s\S]{0,900}recoveryAllowed\(\)[\s\S]{0,900}mounted_[\s\S]{0,900}recoveryPending_') {
  throw 'Retry storage request is not lifecycle/health/pending gated'
}
if ($owner -notmatch 'consumeRecoveryRequest\(\)[\s\S]{0,500}recoveryPending_\s*=\s*false') {
  throw 'Retry storage request is not consumed exactly once'
}

$routeStart = $browser.IndexOf('retryStorageRoute(')
$routeEnd = $browser.IndexOf('static const char* storageHealthName', $routeStart)
if ($routeStart -lt 0 -or $routeEnd -le $routeStart) { throw 'Retry storage route is missing' }
$route = $browser.Substring($routeStart, $routeEnd - $routeStart)
foreach ($marker in @('RaceDirectorSmug', 'current().lifecycle', 'requestRecovery()', 'RECOVERY_REQUESTED',
                      'RECOVERY_ALREADY_PENDING', 'STORAGE_ALREADY_HEALTHY',
                      'RACE_DIRECTOR_REQUIRED', 'Cache-Control')) {
  if ($route -notmatch [regex]::Escape($marker)) { throw "Retry storage route marker missing: $marker" }
}
if ($route -match 'SD\.begin|SD\.open|mount|reconstruct|recover\(') {
  throw 'Retry storage HTTP action performs physical/storage work itself'
}
if ($browser -notmatch '"/request/retry-storage"') { throw 'Retry storage route is not registered' }

foreach ($marker in @('recoveryRequestPending()', 'consumeRecoveryRequest()',
                      'explicitRecoveryRequired', 'sdPersistence.recover()',
                      'activeDetail.recover()', 'controllerTraceStorage.recover()',
                      'markRecoveryFailure()')) {
  if ($main -notmatch [regex]::Escape($marker)) { throw "owner service marker missing: $marker" }
}
if ($main -notmatch 'historyReady\s*&&\s*detailReady\s*&&\s*diagnosticsReady') {
  throw 'durable health is not gated on complete reconstruction'
}
if ($main -notmatch 'explicitRecoveryRequired\s*=\s*consumedExplicit') {
  throw 'failed explicit recovery does not require a new explicit request'
}

if (($psf + $acceptance) -notmatch 'PSF\.38|Retry storage') { throw 'missing documented PSF.38 Retry storage contract' }
if ($stage15 -notmatch 'PS15\.93') { throw 'missing documented PS15.93 Retry storage contract' }
Write-Host 'Retry storage contract PASS authority/lifecycle/one-shot/reconstruction/no-consumer-mount=1'
