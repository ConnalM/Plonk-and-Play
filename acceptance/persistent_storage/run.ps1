$ErrorActionPreference='Stop'
$root=Resolve-Path (Join-Path $PSScriptRoot '..\..')
$include=Join-Path $root 'firmware\include'
$foundation=Join-Path $env:TEMP 'pp-foundation-test.exe'
$transactional=Join-Path $env:TEMP 'pp-transactional-test.exe'
g++ -std=c++17 -Wall -Wextra -I $include (Join-Path $PSScriptRoot 'foundation_test.cpp') -o $foundation
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage Foundation foundation compile failed: $LASTEXITCODE" }
& $foundation
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage Foundation foundation acceptance failed: $LASTEXITCODE" }
g++ -std=c++17 -Wall -Wextra -I $include (Join-Path $PSScriptRoot 'transactional_test.cpp') -o $transactional
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage Foundation transactional compile failed: $LASTEXITCODE" }
& $transactional
if ($LASTEXITCODE -ne 0) { throw "Persistent Storage Foundation transactional acceptance failed: $LASTEXITCODE" }
$engine=Get-Content (Join-Path $root 'firmware\include\pp\race_engine.h') -Raw
$codec=Get-Content (Join-Path $root 'firmware\include\pp\persistent_storage.h') -Raw
$history=Get-Content (Join-Path $root 'firmware\include\pp\history_store.h') -Raw
$boundaryOk=($engine -notmatch 'Preferences|SPIFFS|SD\.h|FFat') -and ($engine -match 'historyPersistFailed_') -and ($engine -match 'storageFaultPublished_') -and ($codec -match 'Magic') -and ($codec -match 'Version') -and ($history -match 'Capacity = 4') -and ($history -notmatch 'PP_MAX_ENTRIES')
if(-not $boundaryOk){throw 'PSF.1/PSF.20 structural boundary check failed'}
Write-Host 'PSF PSF.1 PASS race_engine_uses_storage_boundary=1'
Write-Host 'PSF PSF.20 PASS source_guard_latches_and_suppresses_retry=1'
Write-Host 'PASS: development Persistent Storage Foundation acceptance'
