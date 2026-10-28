$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$owner = Get-Content (Join-Path $root 'firmware\include\pp\physical_sd_owner.h') -Raw
$store = Get-Content (Join-Path $root 'firmware\include\pp\sd_store_arduino.h') -Raw
$core = Get-Content (Join-Path $root 'firmware\include\pp\core.h') -Raw
$history = Get-Content (Join-Path $root 'firmware\include\pp\history_store.h') -Raw
$diagnostic = Get-Content (Join-Path $root 'firmware\include\pp\diagnostic_sd.h') -Raw
$recorder = Get-Content (Join-Path $root 'firmware\include\pp\diagnostic_production.h') -Raw
$browser = Get-Content (Join-Path $root 'firmware\include\pp\browser_interface.h') -Raw
$main = Get-Content (Join-Path $root 'firmware\src\main.cpp') -Raw

if ($owner -notmatch 'void reportIoFailure\(' -or
    $owner -notmatch 'markIoFailureLocked\(' -or
    $owner -notmatch 'mounted_\s*=\s*false;[\s\S]{0,180}health_\s*=\s*Health::Unavailable') {
  throw 'Owner lacks the lease-bound physical I/O failure transition'
}
if ($owner -notmatch 'bool recover\(' -or $owner -notmatch 'recoveryAllowed\(' -or $owner -notmatch 'return mountLocked\(\);') {
  throw 'Owner recovery is no longer lifecycle-gated'
}
if ($owner -notmatch 'commitRecovery\(' -or $owner -notmatch 'durable_\s*=\s*true' -or
    $owner -notmatch 'health_\s*=\s*Health::Healthy') {
  throw 'Owner does not distinguish provisional mount from committed durable health'
}
if ($store -notmatch 'reportPhysicalIoFailure\(' -or
    $store -notmatch 'if \(!file\) \{ reportPhysicalIoFailure\(lease\); return false; \}' -or
    $store -notmatch 'lastHealth_\s*=\s*StoreHealth::Unavailable') {
  throw 'History store does not propagate physical read/write failures'
}
if ($store -notmatch '!owner_\.durable\(\)') {
  throw 'History store can advertise healthy storage before owner recovery commit'
}
if ($core -notmatch 'readRequired\(' -or
    $store -notmatch 'readRequired\(unsigned' -or
    $history -notmatch 'store_\.readRequired\(') {
  throw 'Required History payload reads are not distinguished from optional slot reads'
}
if ($diagnostic -notmatch 'reportPhysicalIoFailure\(' -or
    $diagnostic -notmatch 'owner_\.durable\(\)' -or
    $diagnostic -notmatch 'StorageHealth::Fault' -or
    $diagnostic -notmatch 'value\.durable\s*=\s*owner_\.available\(\)\s*&&\s*owner_\.durable\(\)') {
  throw 'Diagnostics backend does not reflect owner physical unavailability'
}
if ($recorder -notmatch 'const StorageStatus current = storage_\.status\(\)' -or
    $recorder -notmatch 'status\.evidenceVolatile\s*=\s*!current\.durable') {
  throw 'Diagnostic session status does not refresh from backend health'
}
if ($browser -notmatch 'storageDurable' -or $browser -notmatch 'storageHealthName') {
  throw '/health does not expose storage capability health'
}
if ($main -match '(?m)^\s*SD\.begin\(') {
  throw 'Main contains an independent SD.begin call'
}
Write-Host 'SD health propagation PASS owner-transition=1 required-read-boundary=1 recovery-gated=1 diagnostics-visible=1 no-consumer-remount=1'
