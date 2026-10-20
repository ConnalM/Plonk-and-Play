$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tool = Get-Content -Raw (Join-Path $root 'firmware/tools/get-diagnostics.ps1')

foreach ($marker in @(
  "'http://plonk-and-play.local', 'http://192.168.4.1'",
  'network/status',
  'AddSeconds(8)',
  'FromSeconds(1)',
  'Start-Sleep -Milliseconds 750',
  'mDNS route unavailable after bounded retry',
  'explicitAddress')) {
  if ($tool -notlike "*$marker*") { throw "Diagnostics-tool discovery marker missing: $marker" }
}

if ($tool -notmatch '\$defaultDiscovery -and \$candidate -eq \$candidates\[0\]') {
  throw 'mDNS retry is not restricted to default discovery.'
}
if ($tool -notlike '*$candidates = @($BaseUrl.TrimEnd(''*' -or $tool -notlike '*$explicitAddress*') {
  throw 'Explicit -BaseUrl handling no longer uses a single explicit candidate.'
}

Write-Host 'Diagnostics retrieval discovery contract: PASS'
