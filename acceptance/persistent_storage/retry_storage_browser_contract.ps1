$ErrorActionPreference = 'Stop'

$page = Get-Content (Join-Path $PSScriptRoot '..\..\firmware\browser\normal_browser.html') -Raw
$embed = Get-Content (Join-Path $PSScriptRoot '..\..\firmware\browser\embed_normal.py') -Raw
$generated = Get-Content (Join-Path $PSScriptRoot '..\..\firmware\include\pp\normal_browser_page.inc') -Raw

function Assert-Marker([string]$marker, [string]$description) {
    if ($page -notmatch [regex]::Escape($marker) -and $embed -notmatch [regex]::Escape($marker)) {
        throw "Retry storage Browser contract marker missing: $description ($marker)"
    }
}

Assert-Marker 'Retry storage' 'System Health action label'
Assert-Marker '/request/retry-storage' 'production endpoint'
Assert-Marker 'retryStoragePending' 'duplicate-submission guard'
Assert-Marker 'C?.isMaster' 'controlling Browser authority gate'
Assert-Marker "['READY','FINISHED']" 'lifecycle eligibility gate'
Assert-Marker 'health===2||health===3' 'degraded/unavailable visibility gate'
Assert-Marker 'response.status===202&&body.accepted===true' 'bounded response handling'

if ($embed -match 'Storage is unavailable or degraded') {
    throw 'Retry storage panel still conflates physical availability with logical History health'
}
if ($generated -match 'Storage is unavailable or degraded') {
    throw 'Generated Browser payload still contains the conflated recovery wording'
}
Assert-Marker 'physicalUnavailable' 'physical storage availability branch'
Assert-Marker 'historyDegraded' 'logical History degradation branch'
Assert-Marker 'Storage is unavailable.' 'unavailable-storage wording'
Assert-Marker 'History is degraded or corrupt.' 'logical History wording'
Assert-Marker 'Retry storage does not repair History integrity.' 'no false repair implication'
foreach ($marker in @('physicalUnavailable','historyDegraded','Storage is unavailable.','History is degraded or corrupt.','Retry storage does not repair History integrity.')) {
    if ($generated -notmatch [regex]::Escape($marker)) { throw "Generated Browser payload is missing presentation marker: $marker" }
}

# Deterministic presentation contract for the three supported states.  This
# mirrors the controller fields consumed by the Browser and intentionally
# performs no request or storage operation.
function Get-Presentation([string]$physical, [string]$history) {
    $physicalUnavailable = $physical -ne 'HEALTHY' -and $physical -ne 'MOUNTED'
    $historyDegraded = @('CORRUPT','UNSUPPORTED','DEGRADED') -contains $history
    if ($physicalUnavailable) { return 'STORAGE_UNAVAILABLE' }
    if ($historyDegraded) { return 'HISTORY_DEGRADED' }
    return 'NO_RECOVERY_WARNING'
}
if ((Get-Presentation 'HEALTHY' 'HEALTHY') -ne 'NO_RECOVERY_WARNING') { throw 'Healthy storage must not show recovery warning' }
if ((Get-Presentation 'MOUNTED' 'CORRUPT') -ne 'HISTORY_DEGRADED') { throw 'Logical History degradation must remain distinct' }
if ((Get-Presentation 'UNKNOWN' 'UNKNOWN') -ne 'STORAGE_UNAVAILABLE') { throw 'Unavailable physical storage must show recovery state' }

if ($embed -notmatch 'replace_once\("</script>", retry_storage_js') {
    throw 'Retry storage UI is not injected into the generated production Browser page'
}
if ($page -match 'SD\.begin|mount\(') {
    throw 'Browser source contains storage-owner operations'
}

Write-Output 'Retry storage Browser contract: PASS'
