$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$detail = Get-Content (Join-Path $root 'firmware\include\pp\sd_store_arduino.h') -Raw
$owner = Get-Content (Join-Path $root 'firmware\include\pp\physical_sd_owner.h') -Raw
$main = Get-Content (Join-Path $root 'firmware\src\main.cpp') -Raw

# The instrumentation is intentionally structural and observation-only.  It
# must expose each bounded finalisation boundary and the owner/consumer
# recovery states without adding a second persistence path or credential data.
foreach ($marker in @(
  'detail_finalize begin',
  'detail_finalize failed stage=drain',
  'detail_finalize failed stage=mount',
  'detail_finalize failed stage=seal',
  'detail_finalize success',
  'detail_trace stage=',
  'detailTrace("finalize", "drain_begin"',
  'detailTrace("finalize", "drain_end"',
  'detailTrace("write","lease"',
  'detailTrace("seal","marker_open"',
  'detailTrace("verify","marker"',
  'detail_write_open entry=',
  'detail_write_header entry=',
  'detail_write_record entry=',
  'detail_write_flush entry=',
  'const char* mode=exists?FILE_APPEND:FILE_WRITE',
  'detail_drain write_failed',
  'detail_recover before',
  'detail_recover result',
  'History store recover',
  'history_boundary=',
  'history_reconstruction_attempted='
)) {
  if (($detail + $main) -notmatch [regex]::Escape($marker)) {
    throw "Missing persistence instrumentation marker: $marker"
  }
}
if ($owner -notmatch 'SD owner recover request' -or $owner -notmatch 'SD owner recover result') {
  throw 'Missing physical SD owner recovery instrumentation'
}

# No new instrumentation may print or fingerprint credentials.
if (($detail + $owner + $main) -match '(?i)password|credential|ssid|secret|token') {
  throw 'Persistence instrumentation contains a forbidden credential-related field or label'
}

Write-Host 'WP5 persistence instrumentation contract: PASS bounded stages/owner recovery/privacy'
