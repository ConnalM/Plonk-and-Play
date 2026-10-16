$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$include=Join-Path $root 'firmware\include'
$foundation=Join-Path $env:TEMP 'pp-foundation-test.exe'
$transactional=Join-Path $env:TEMP 'pp-transactional-test.exe'
$production=Join-Path $env:TEMP 'pp-production-history-test.exe'
$detailFailure=Join-Path $env:TEMP 'pp-active-detail-failure-test.exe'
$detailStream=Join-Path $env:TEMP 'pp-active-detail-stream-test.exe'
$detailStreamFailure=Join-Path $env:TEMP 'pp-active-detail-stream-failure-test.exe'
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
$engine=Get-Content (Join-Path $root 'firmware\include\pp\race_engine.h') -Raw
$codec=Get-Content (Join-Path $root 'firmware\include\pp\persistent_storage.h') -Raw
$history=Get-Content (Join-Path $root 'firmware\include\pp\history_store.h') -Raw
$boundaryOk=($engine -notmatch 'Preferences|SPIFFS|SD\.h|FFat') -and ($engine -match 'historyPersistFailed_') -and ($engine -match 'storageFaultPublished_') -and ($codec -match 'Magic') -and ($codec -match 'Version') -and ($history -match 'Capacity = 100') -and ($history -notmatch 'MaxBytes = 8192') -and ($history -notmatch 'PP_MAX_ENTRIES')
if(-not $boundaryOk){throw 'PSF.1/PSF.20 structural boundary check failed'}
Write-Host 'PSF PSF.1 PASS race_engine_uses_storage_boundary=1'
Write-Host 'PSF PSF.20 PASS source_guard_latches_and_suppresses_retry=1'
Write-Host 'PASS: development Persistent Storage Foundation acceptance'
