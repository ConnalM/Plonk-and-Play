$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$history = Get-Content (Join-Path $root 'firmware\include\pp\history_store.h') -Raw
$versioned = Get-Content (Join-Path $root 'firmware\include\pp\persistent_storage.h') -Raw
$sd = Get-Content (Join-Path $root 'firmware\include\pp\sd_store_arduino.h') -Raw

foreach ($marker in @(
  'historyCommitTrace("append_detailed", "encode"',
  'historyCommitTrace("append_detailed", "backend"',
  'historyCommitTrace("slot_append", "payload_verify"',
  'historyCommitTrace("slot_append", "metadata_publish"',
  'historyCommitTrace("metadata_publish", "write"',
  'historyCommitTrace("metadata_publish", "verify"'
)) {
  if ($history -notmatch [regex]::Escape($marker) -and $versioned -notmatch [regex]::Escape($marker)) {
    throw "Missing History append diagnostic marker: $marker"
  }
}
foreach ($marker in @(
  'history_store_write stage=temp_open',
  'history_store_write stage=temp_write',
  'history_store_write stage=temp_verify_open_size',
  'history_store_write stage=temp_verify_read',
  'history_store_write stage=rename',
  'history_store_write stage=success'
)) {
  if ($sd -notmatch [regex]::Escape($marker)) { throw "Missing SD write diagnostic marker: $marker" }
}
Write-Host 'PSF History append instrumentation contract: PASS boundary_markers=1 backend_write_stages=1'
