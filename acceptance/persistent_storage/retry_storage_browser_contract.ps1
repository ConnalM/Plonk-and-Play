$ErrorActionPreference = 'Stop'

$page = Get-Content (Join-Path $PSScriptRoot '..\..\firmware\browser\normal_browser.html') -Raw
$embed = Get-Content (Join-Path $PSScriptRoot '..\..\firmware\browser\embed_normal.py') -Raw

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

if ($embed -notmatch 'replace_once\("</script>", retry_storage_js') {
    throw 'Retry storage UI is not injected into the generated production Browser page'
}
if ($page -match 'SD\.begin|mount\(') {
    throw 'Browser source contains storage-owner operations'
}

Write-Output 'Retry storage Browser contract: PASS'
