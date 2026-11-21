[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^http://')]
    [string]$TasterUrl,

    [Parameter(Mandatory = $true)]
    [switch]$Acceptance,

    [Parameter(Mandatory = $true)]
    [string]$EvidenceDirectory
)

<#
Runs the approved final WP8-02 physical verification sequence.

This script has no default endpoint and creates a new evidence directory.  It
captures the proposal read-only before Stage 15, runs Stage 15, Stage 13 and
Stage 8 in that order, and stops immediately on the first failure.  Each
successful stage is followed by a read-only comparison against the immutable
pre-suite proposal.  The observer must independently witness the labelled
physical display states printed by Stage 8; HTTP evidence does not prove pixels.
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $Acceptance) {
    throw 'Explicit -Acceptance is required; this must only be run against the authorised physical acceptance image.'
}
if (Test-Path -LiteralPath $EvidenceDirectory) {
    throw "Evidence directory already exists; refusing to overwrite: $EvidenceDirectory"
}

$root = $PSScriptRoot
$python = Get-Command python -ErrorAction Stop
$baseline = Join-Path $EvidenceDirectory 'proposal-baseline.json'
New-Item -ItemType Directory -Path $EvidenceDirectory -ErrorAction Stop | Out-Null

function Invoke-PhysicalStage {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Script
    )

    $log = Join-Path $EvidenceDirectory "$Name.log"
    "=== $Name ===" | Tee-Object -FilePath $log
    & $python.Source $Script $TasterUrl --acceptance 2>&1 | Tee-Object -FilePath $log -Append
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed; later stages were not started. Preserve $EvidenceDirectory for review."
    }
    & $python.Source (Join-Path $root 'physical_acceptance_baseline.py') verify $TasterUrl --acceptance $baseline 2>&1 |
        Tee-Object -FilePath $log -Append
    if ($LASTEXITCODE -ne 0) {
        throw "$Name did not restore the immutable proposal baseline. Stop for review; do not overwrite settings."
    }
}

$previousBaseline = $env:PP_ACCEPTANCE_BASELINE
try {
    # This initial request is read-only.  It refuses to replace existing evidence.
    & $python.Source (Join-Path $root 'physical_acceptance_baseline.py') capture $TasterUrl --acceptance $baseline
    if ($LASTEXITCODE -ne 0) {
        throw 'Baseline capture failed before any physical acceptance action.'
    }
    $env:PP_ACCEPTANCE_BASELINE = (Resolve-Path -LiteralPath $baseline).Path

    Invoke-PhysicalStage -Name 'stage15' -Script (Join-Path $root 'stage15\multiclient_http_acceptance.py')
    Invoke-PhysicalStage -Name 'stage13' -Script (Join-Path $root 'stage13\http_acceptance.py')
    Invoke-PhysicalStage -Name 'stage8' -Script (Join-Path $root 'stage8\physical_taster_simulation_acceptance.py')

    Write-Host "WP8-02 scripted physical verification passed. Confirm the independent Stage 8 screen observation before signing off."
}
finally {
    if ($null -eq $previousBaseline) {
        Remove-Item Env:PP_ACCEPTANCE_BASELINE -ErrorAction SilentlyContinue
    }
    else {
        $env:PP_ACCEPTANCE_BASELINE = $previousBaseline
    }
}
