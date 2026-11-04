$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& python (Join-Path $PSScriptRoot 'racer_browser_contract.py')
if ($LASTEXITCODE -ne 0) { throw "WP7.3 Racer Browser contract failed: $LASTEXITCODE" }

$generatedPath = Join-Path $root 'firmware\include\pp\normal_browser_page.inc'
$parseOutput = & node (Join-Path $PSScriptRoot 'racer_browser_parse.js') $generatedPath 2>&1
if ($LASTEXITCODE -ne 0) { throw "Generated Racer Browser JavaScript parse failed: $parseOutput" }

Write-Host 'WP7.3 Racer Browser contract PASS'
