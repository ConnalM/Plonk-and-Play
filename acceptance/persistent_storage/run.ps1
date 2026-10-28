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
$ownerContract=Join-Path $PSScriptRoot 'physical_sd_owner_contract.ps1'
$retryStorageBrowserContract=Join-Path $PSScriptRoot 'retry_storage_browser_contract.ps1'
$healthContract=Join-Path $PSScriptRoot 'storage_health_propagation_contract.ps1'
$retryStorageContract=Join-Path $PSScriptRoot 'retry_storage_contract.ps1'
$recoveryPolicy=Join-Path $env:TEMP 'pp-retry-storage-recovery-policy.exe'
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
& $ownerContract
if ($LASTEXITCODE -ne 0) { throw "Physical SD owner contract acceptance failed: $LASTEXITCODE" }
& $healthContract
if ($LASTEXITCODE -ne 0) { throw "Storage health propagation contract acceptance failed: $LASTEXITCODE" }
& $retryStorageContract
if ($LASTEXITCODE -ne 0) { throw "Retry storage contract acceptance failed: $LASTEXITCODE" }
& $retryStorageBrowserContract
if ($LASTEXITCODE -ne 0) { throw "Retry storage Browser contract acceptance failed: $LASTEXITCODE" }
g++ -std=c++17 -Wall -Wextra -Werror (Join-Path $PSScriptRoot 'recovery_policy_test.cpp') -o $recoveryPolicy
if ($LASTEXITCODE -ne 0) { throw "Retry storage recovery-policy compile failed: $LASTEXITCODE" }
& $recoveryPolicy
if ($LASTEXITCODE -ne 0) { throw "Retry storage recovery-policy acceptance failed: $LASTEXITCODE" }
$engine=Get-Content (Join-Path $root 'firmware\include\pp\race_engine.h') -Raw
$codec=Get-Content (Join-Path $root 'firmware\include\pp\persistent_storage.h') -Raw
$history=Get-Content (Join-Path $root 'firmware\include\pp\history_store.h') -Raw
$sd=Get-Content (Join-Path $root 'firmware\include\pp\sd_store_arduino.h') -Raw
$boundaryOk=($engine -notmatch 'Preferences|SPIFFS|SD\.h|FFat') -and ($engine -match 'historyPersistFailed_') -and ($engine -match 'storageFaultPublished_') -and ($codec -match 'Magic') -and ($codec -match 'Version') -and ($history -match 'Capacity = 100') -and ($history -notmatch 'MaxBytes = 8192') -and ($history -notmatch 'PP_MAX_ENTRIES')
if(-not $boundaryOk){throw 'PSF.1/PSF.20 structural boundary check failed'}
Write-Host 'PSF PSF.1 PASS race_engine_uses_storage_boundary=1'
Write-Host 'PSF PSF.20 PASS source_guard_latches_and_suppresses_retry=1'
if(($sd -notmatch 'cleanupOrphans') -or ($sd -notmatch 'markerPath') -or ($sd -notmatch 'storagePending_') -or ($sd -notmatch 'detailLost')){throw 'WP2D active stream recovery boundary check failed'}
Write-Host 'WP2D D2 PASS orphan_cleanup_integrity_marker_recovery_boundary=1'
if(($history -notmatch 'reclaim\(') -or ($sd -notmatch 'cleanupSlotTemps') -or ($sd -notmatch 'ProvisionalLowFreeBytes')){throw 'WP2E storage housekeeping boundary check failed'}
Write-Host 'WP2E structural PASS bounded_reclamation_capacity_guard_temp_cleanup=1'
Write-Host 'PASS: development Persistent Storage Foundation acceptance'
