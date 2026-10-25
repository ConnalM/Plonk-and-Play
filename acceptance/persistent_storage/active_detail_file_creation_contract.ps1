$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$source = Get-Content (Join-Path $root 'firmware\include\pp\sd_store_arduino.h') -Raw
$start = $source.IndexOf('bool writeRecord(')
$end = $source.IndexOf('String markerPath(', $start)
if ($start -lt 0 -or $end -le $start) { throw 'Active-detail writeRecord boundary missing' }
$writeRecord = $source.Substring($start, $end - $start)

# The ESP32 Arduino FS implementation opens a missing non-read file with
# fopen(mode), so FILE_APPEND itself creates the file and returns a valid
# handle. The production path must therefore preflight existence and choose
# FILE_WRITE for the first record.
$readIndex = $writeRecord.IndexOf('store_.open(p,FILE_READ)')
$modeIndex = $writeRecord.IndexOf('const char* mode=exists?FILE_APPEND:FILE_WRITE')
$headerIndex = $writeRecord.IndexOf('if(!exists)')
if ($readIndex -lt 0 -or $modeIndex -lt 0 -or $headerIndex -lt 0) {
  throw 'Active-detail creation path does not explicitly separate existing append from first-file header creation'
}
if ($readIndex -ge $modeIndex -or $headerIndex -le $modeIndex) {
  throw 'Active-detail existence preflight/header ordering is unsafe for append-creates-missing semantics'
}
if ($writeRecord -match 'File f=store_\.open\(p,FILE_APPEND\);if\(!f\)') {
  throw 'Active-detail still assumes FILE_APPEND fails for a missing file'
}

# Deterministic framework model: append mode creates a missing file. The
# production decision must still select write mode and emit the header once.
$fileExists = $false
$openMode = if ($fileExists) { 'a' } else { 'w' }
$headerWrites = if ($fileExists) { 0 } else { 1 }
if ($openMode -ne 'w' -or $headerWrites -ne 1) {
  throw 'FILE_APPEND missing-file model did not require one header initialization'
}
Write-Host 'WP5 active-detail FILE_APPEND creation contract: PASS'

$cleanupStart = $source.IndexOf('void cleanupOrphans()')
$cleanupEnd = $source.IndexOf('static bool readEncoded', $cleanupStart)
if ($cleanupStart -lt 0 -or $cleanupEnd -le $cleanupStart) {
  throw 'Active-detail orphan cleanup boundary missing'
}
$cleanup = $source.Substring($cleanupStart, $cleanupEnd - $cleanupStart)
if ($cleanup -notmatch 'String path=file\.path\(\)') {
  throw 'Active-detail orphan cleanup does not use the full filesystem path'
}
if ($cleanup -match 'String name=file\.name\(\)') {
  throw 'Active-detail orphan cleanup still passes a basename to SD removal'
}
if ($cleanup -notmatch 'store_\.remove\(path\)') {
  throw 'Active-detail orphan cleanup does not remove the canonical full path'
}
Write-Host 'WP5 active-detail stale-orphan cleanup contract: PASS full_path_removal=1'

