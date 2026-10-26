$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$include=Join-Path $root 'firmware\include'
$foundation=Join-Path $env:TEMP 'pp-foundation-test.exe'
$transactional=Join-Path $env:TEMP 'pp-transactional-test.exe'
$production=Join-Path $env:TEMP 'pp-production-history-test.exe'
$detailFailure=Join-Path $env:TEMP 'pp-active-detail-failure-test.exe'
$detailStream=Join-Path $env:TEMP 'pp-active-detail-stream-test.exe'
$detailStreamFailure=Join-Path $env:TEMP 'pp-active-detail-stream-failure-test.exe'
$detailRecovery=Join-Path $env:TEMP 'pp-active-detail-recovery-test.exe'
$housekeeping=Join-Path $env:TEMP 'pp-storage-housekeeping-test.exe'
$semantic=Join-Path $env:TEMP 'pp-wp2f-semantic-test.exe'
$reconstruction=Join-Path $env:TEMP 'pp-reconstruction-readiness-test.exe'
$trackRecordAb=Join-Path $env:TEMP 'pp-track-record-ab-test.exe'
$payloadDump=Join-Path $env:TEMP 'pp-history-payload-dump-test.exe'
$ownerContract=Join-Path $PSScriptRoot 'physical_sd_owner_contract.ps1'
$fileCreationContract=Join-Path $PSScriptRoot 'active_detail_file_creation_contract.ps1'
$metadataWorkspaceContract=Join-Path $PSScriptRoot 'history_metadata_workspace_contract.ps1'
g++ -std=c++17 -Wall -Wextra -I $include (Join-Path $PSScriptRoot 'foundation_test.cpp') -o $foundation
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage Foundation foundation compile failed: $LASTEXITCODE" }
& $foundation
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage Foundation foundation acceptance failed: $LASTEXITCODE" }
g++ -std=c++17 -Wall -Wextra -I $include (Join-Path $PSScriptRoot 'transactional_test.cpp') -o $transactional
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage Foundation transactional compile failed: $LASTEXITCODE" }
& $transactional
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage Foundation transactional acceptance failed: $LASTEXITCODE" }
g++ -std=c++17 -Wall -Wextra -I $include (Join-Path $PSScriptRoot 'production_history_test.cpp') -o $production
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage Foundation production-history compile failed: $LASTEXITCODE" }
& $production
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage Foundation production-history acceptance failed: $LASTEXITCODE" }
g++ -std=c++17 -Wall -Wextra -I $include (Join-Path $PSScriptRoot 'active_detail_test.cpp') -o (Join-Path $env:TEMP 'pp-active-detail-test.exe')
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage active-detail compile failed: $LASTEXITCODE" }
& (Join-Path $env:TEMP 'pp-active-detail-test.exe')
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage active-detail acceptance failed: $LASTEXITCODE" }
g++ -std=c++17 -Wall -Wextra -I $include (Join-Path $PSScriptRoot 'active_detail_failure_test.cpp') -o $detailFailure
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage active-detail failure compile failed: $LASTEXITCODE" }
& $detailFailure
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage active-detail failure acceptance failed: $LASTEXITCODE" }
g++ -std=c++17 -Wall -Wextra -I $include (Join-Path $PSScriptRoot 'active_detail_stream_test.cpp') -o $detailStream
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage active-detail stream compile failed: $LASTEXITCODE" }
& $detailStream
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage active-detail stream acceptance failed: $LASTEXITCODE" }
g++ -std=c++17 -Wall -Wextra -I $include (Join-Path $PSScriptRoot 'active_detail_stream_failure_test.cpp') -o $detailStreamFailure
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage active-detail stream failure compile failed: $LASTEXITCODE" }
& $detailStreamFailure
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage active-detail stream failure acceptance failed: $LASTEXITCODE" }
g++ -std=c++17 -Wall -Wextra -I $include (Join-Path $PSScriptRoot 'active_detail_recovery_test.cpp') -o $detailRecovery
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage active-detail recovery compile failed: $LASTEXITCODE" }
& $detailRecovery
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage active-detail recovery acceptance failed: $LASTEXITCODE" }
g++ -std=c++17 -Wall -Wextra -I $include (Join-Path $PSScriptRoot 'storage_housekeeping_test.cpp') -o $housekeeping
if ($LASTEXITCODE -ne 0) { throw "WP2E storage-housekeeping compile failed: $LASTEXITCODE" }
& $housekeeping
if ($LASTEXITCODE -ne 0) { throw "WP2E storage-housekeeping acceptance failed: $LASTEXITCODE" }
g++ -std=c++17 -Wall -Wextra -I $include (Join-Path $PSScriptRoot 'wp2f_semantic_test.cpp') -o $semantic
if ($LASTEXITCODE -ne 0) { throw "WP2F semantic compile failed: $LASTEXITCODE" }
& $semantic
if ($LASTEXITCODE -ne 0) { throw "WP2F semantic acceptance failed: $LASTEXITCODE" }
g++ -std=c++17 -Wall -Wextra -I $include (Join-Path $PSScriptRoot 'reconstruction_readiness_test.cpp') -o $reconstruction
if ($LASTEXITCODE -ne 0) { throw "History reconstruction-readiness compile failed: $LASTEXITCODE" }
& $reconstruction
if ($LASTEXITCODE -ne 0) { throw "History reconstruction-readiness acceptance failed: $LASTEXITCODE" }
g++ -std=c++17 -Wall -Wextra -I $include (Join-Path $PSScriptRoot 'track_record_ab_test.cpp') -o $trackRecordAb
if ($LASTEXITCODE -ne 0) { throw "Track Record A/B compile failed: $LASTEXITCODE" }
& $trackRecordAb
if ($LASTEXITCODE -ne 0) { throw "Track Record A/B acceptance failed: $LASTEXITCODE" }
g++ -std=c++17 -Wall -Wextra -I $include (Join-Path $PSScriptRoot 'history_payload_dump_test.cpp') -o $payloadDump
if ($LASTEXITCODE -ne 0) { throw "History payload dump compile failed: $LASTEXITCODE" }
& $payloadDump
if ($LASTEXITCODE -ne 0) { throw "History payload dump acceptance failed: $LASTEXITCODE" }
& $ownerContract
if ($LASTEXITCODE -ne 0) { throw "Physical SD owner contract acceptance failed: $LASTEXITCODE" }
& $fileCreationContract
if ($LASTEXITCODE -ne 0) { throw "Active-detail FILE_APPEND creation contract acceptance failed: $LASTEXITCODE" }
& $metadataWorkspaceContract
if ($LASTEXITCODE -ne 0) { throw "History metadata workspace contract acceptance failed: $LASTEXITCODE" }
$engine=Get-Content (Join-Path $root 'firmware\include\pp\race_engine.h') -Raw
$core=Get-Content (Join-Path $root 'firmware\include\pp\core.h') -Raw
$codec=Get-Content (Join-Path $root 'firmware\include\pp\persistent_storage.h') -Raw
$history=Get-Content (Join-Path $root 'firmware\include\pp\history_store.h') -Raw
$sd=Get-Content (Join-Path $root 'firmware\include\pp\sd_store_arduino.h') -Raw
$boundaryOk=($engine -notmatch 'Preferences|SPIFFS|SD\.h|FFat') -and ($engine -match 'historyPersistFailed_') -and ($engine -match 'storageFaultPublished_') -and ($codec -match 'Magic') -and ($codec -match 'Version') -and ($history -match 'Capacity = 100') -and ($history -notmatch 'MaxBytes = 8192') -and ($history -notmatch 'PP_MAX_ENTRIES')
if(-not $boundaryOk){throw 'PSF.1/PSF.20 structural boundary check failed'}
if(($core -notmatch 'writeTrackRecord') -or ($codec -notmatch 'AlternateSlot') -or ($sd -notmatch 'writeTrackRecord')){throw 'Track Record A/B publication boundary check failed'}
Write-Host 'PSF TrackRecord PASS dedicated_AB_publication_boundary=1'
Write-Host 'PSF PSF.1 PASS race_engine_uses_storage_boundary=1'
Write-Host 'PSF PSF.20 PASS source_guard_latches_and_suppresses_retry=1'
if(($sd -notmatch 'cleanupOrphans') -or ($sd -notmatch 'markerPath') -or ($sd -notmatch 'storagePending_') -or ($sd -notmatch 'detailLost')){throw 'WP2D active stream recovery boundary check failed'}
Write-Host 'WP2D D2 PASS orphan_cleanup_integrity_marker_recovery_boundary=1'
if(($history -notmatch 'reclaim\(') -or ($sd -notmatch 'cleanupSlotTemps') -or ($sd -notmatch 'ProvisionalLowFreeBytes')){throw 'WP2E storage housekeeping boundary check failed'}
Write-Host 'WP2E structural PASS bounded_reclamation_capacity_guard_temp_cleanup=1'
Write-Host 'PASS: development Persistent Storage Foundation acceptance'
