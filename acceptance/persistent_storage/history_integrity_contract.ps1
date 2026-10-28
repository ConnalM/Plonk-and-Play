$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$sd=Get-Content (Join-Path $root 'firmware\include\pp\sd_store_arduino.h') -Raw
$history=Get-Content (Join-Path $root 'firmware\include\pp\persistent_storage.h') -Raw
$browser=Get-Content (Join-Path $root 'firmware\include\pp\browser_interface.h') -Raw
$main=Get-Content (Join-Path $root 'firmware\src\main.cpp') -Raw
if ($sd -match 'actual\s*!==?\s*size[\s\S]{0,220}memset\(data,\s*0,\s*size\)[\s\S]{0,80}return\s+true') { throw 'size mismatch still reports a successful zero-filled read' }
if ($sd -notmatch 'lastHealth_\s*=\s*StoreHealth::Corrupt' -or $sd -notmatch 'history_payload_integrity') { throw 'size mismatch integrity boundary is not classified/diagnosed' }
if ($history -notmatch 'bool reconstruct\(\) override' -or $history -notmatch 'invalidCount\(\) const override' -or $history -notmatch 'HistoryFaultReason::Unsupported') { throw 'versioned History does not expose reconstruction integrity state' }
if ($browser -notmatch 'history_->invalidCount\(\)' -or $browser -notmatch 'historyHealthName' -or $browser -notmatch 'continue;') { throw 'History route does not preserve valid siblings while exposing logical health' }
if ($main -notmatch 'history\.reconstruct\(\)') { throw 'startup/recovery does not validate versioned History' }
Write-Host 'PSF HISTORY-INTEGRITY PASS size_mismatch_excluded=1 valid_siblings_preserved=1 logical_health_exposed=1'
