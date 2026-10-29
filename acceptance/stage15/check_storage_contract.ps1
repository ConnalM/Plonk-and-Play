$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$owner = Get-Content -Raw (Join-Path $root 'firmware/include/pp/physical_sd_owner.h')
$interface = Get-Content -Raw (Join-Path $root 'firmware/include/pp/browser_interface.h')
$embed = Get-Content -Raw (Join-Path $root 'firmware/browser/embed_normal.py')

function Section([string]$text, [string]$start, [string]$end) {
  $a = $text.IndexOf($start)
  $b = $text.IndexOf($end, $a + $start.Length)
  if ($a -lt 0 -or $b -lt $a) { throw "section not found: $start .. $end" }
  return $text.Substring($a, $b - $a)
}

$check = Section $owner 'StorageCheckResult checkStorage()' 'bool consumeRecoveryRequest()'
if (([regex]::Matches($check, [regex]::Escape('open("/pp-p1", FILE_READ)')).Count) -ne 1) {
  throw 'Check storage must perform exactly one deterministic read-only filesystem open'
}
foreach ($marker in @('StorageCheckResult::Accessible','StorageCheckResult::PhysicalFailure','lease.reportIoFailure()','root.isDirectory()')) {
  if ($check -notlike "*$marker*") { throw "Owner check marker missing: $marker" }
}
foreach ($forbidden in @('SD.begin','mountLocked','recover(','FILE_WRITE','write(','remove(','rename(','mkdir(')) {
  if ($check -like "*$forbidden*") { throw "Owner check contains forbidden operation: $forbidden" }
}

$route = Section $interface 'static esp_err_t checkStorageRoute' 'static const char* physicalStorageHealthName'
foreach ($marker in @('ClientContext::RaceDirectorSmug','SessionLifecycle::Ready','SessionLifecycle::Finished','checkStorage()','PHYSICAL_FAILURE','ALREADY_PENDING','physicalStorageDurable','historyHealthName')) {
  if ($route -notlike "*$marker*") { throw "Check storage route marker missing: $marker" }
}
if (([regex]::Matches($route, [regex]::Escape('checkStorage()')).Count) -ne 1) { throw 'Accepted request does not map to exactly one owner check' }
foreach ($forbidden in @('requestRecovery','SD.begin','mountLocked','recover(','FILE_WRITE')) {
  if ($route -like "*$forbidden*") { throw "Check route contains forbidden recovery/write operation: $forbidden" }
}

foreach ($marker in @('checkStoragePending','/request/check-storage','CHECK STORAGE','C?.isMaster===true','READY','FINISHED','await diagnosticsPage()','physicalStorage','physicalDurable','historyHealth')) {
  if ($embed -notlike "*$marker*") { throw "Check storage Browser marker missing: $marker" }
}

function Invoke-CheckModel([string]$context, [string]$lifecycle, [bool]$mounted,
                           [bool]$mediumReadable, [string]$historyHealth,
                           [bool]$pending) {
  if ($context -ne 'MASTER') { return @{ result='UNAUTHORIZED'; attempts=0; mounted=$mounted; durable=$mounted; history=$historyHealth } }
  if ($lifecycle -notin @('READY','FINISHED')) { return @{ result='LIFECYCLE_REJECTED'; attempts=0; mounted=$mounted; durable=$mounted; history=$historyHealth } }
  if ($pending) { return @{ result='ALREADY_PENDING'; attempts=0; mounted=$mounted; durable=$mounted; history=$historyHealth } }
  if (-not $mounted) { return @{ result='UNAVAILABLE'; attempts=0; mounted=$false; durable=$false; history=$historyHealth } }
  if (-not $mediumReadable) { return @{ result='PHYSICAL_FAILURE'; attempts=1; mounted=$false; durable=$false; history='UNAVAILABLE' } }
  return @{ result='ACCESSIBLE'; attempts=1; mounted=$true; durable=$true; history=$historyHealth }
}

$healthy = Invoke-CheckModel MASTER READY $true $true HEALTHY $false
if ($healthy.result -ne 'ACCESSIBLE' -or $healthy.attempts -ne 1 -or -not $healthy.durable) { throw 'Healthy check model failed' }
$missing = Invoke-CheckModel MASTER READY $true $false HEALTHY $false
if ($missing.result -ne 'PHYSICAL_FAILURE' -or $missing.attempts -ne 1 -or $missing.mounted -or $missing.durable) { throw 'Missing-medium degradation model failed' }
$logical = Invoke-CheckModel MASTER FINISHED $true $true CORRUPT $false
if ($logical.result -ne 'ACCESSIBLE' -or $logical.history -ne 'CORRUPT') { throw 'Physical success masked logical History degradation' }
$spectator = Invoke-CheckModel SPECTATOR READY $true $true HEALTHY $false
if ($spectator.result -ne 'UNAUTHORIZED' -or $spectator.attempts -ne 0) { throw 'Spectator check was not rejected' }
$racing = Invoke-CheckModel MASTER RACING $true $true HEALTHY $false
if ($racing.result -ne 'LIFECYCLE_REJECTED' -or $racing.attempts -ne 0) { throw 'Active-race check was not rejected' }
$duplicate = Invoke-CheckModel MASTER READY $true $true HEALTHY $true
if ($duplicate.result -ne 'ALREADY_PENDING' -or $duplicate.attempts -ne 0) { throw 'Duplicate check was not bounded' }

Write-Host 'PSF.39 / PS15.94 Check storage contract: PASS'
