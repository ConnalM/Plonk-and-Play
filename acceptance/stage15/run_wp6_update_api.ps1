$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $PSScriptRoot 'wp6_update_api_test.exe'
$src = Join-Path $PSScriptRoot 'wp6_update_api_test.cpp'
$include = Join-Path $root '..\firmware\include'
& g++ -std=c++17 -Wall -Wextra -Werror -I $include $src -o $out
if ($LASTEXITCODE -ne 0) { throw 'WP6 update API contract compile failed' }
& $out
if ($LASTEXITCODE -ne 0) { throw 'WP6 update API contract failed' }
$browser = Get-Content (Join-Path $root '..\firmware\include\pp\browser_interface.h') -Raw
foreach ($route in @('/update/status','/request/update/begin','/request/update/upload','/request/update/validate','/request/update/install')) {
  if ($browser.IndexOf($route, [System.StringComparison]::Ordinal) -lt 0) { throw "Missing update route: $route" }
}
if ($browser.IndexOf('FirmwareUpdateEndpoint', [System.StringComparison]::Ordinal) -lt 0) { throw 'Browser update endpoint seam missing' }
Write-Output 'WP6_UPDATE_API_ROUTES_PASS'
