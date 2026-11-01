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
foreach ($route in @('/update/status','/update/check-online','/request/update/begin','/request/update/upload','/request/update/validate','/request/update/install','/request/update/online','/request/restart','/request/reset-settings','/request/erase-everything')) {
  if ($browser.IndexOf($route, [System.StringComparison]::Ordinal) -lt 0) { throw "Missing update route: $route" }
}
if ($browser.IndexOf('FirmwareUpdateEndpoint', [System.StringComparison]::Ordinal) -lt 0) { throw 'Browser update endpoint seam missing' }
foreach ($marker in @('const bool controlling = browser->isControlOwner(owner);', 'const bool settled = state.lifecycle == SessionLifecycle::Ready || state.lifecycle == SessionLifecycle::Finished;', 'FirmwareUpdateRequestContext context{};', 'install request received authority=', 'install request %s reason=', 'install request rejected')) {
  if ($browser.IndexOf($marker, [System.StringComparison]::Ordinal) -lt 0) { throw "Install authority/observability marker missing: $marker" }
}
$adapter = Get-Content (Join-Path $root '..\firmware\include\pp\esp32_firmware_update_adapter.h') -Raw
foreach ($marker in @('install boot-select attempted', 'install boot-select accepted', 'install restart requested')) {
  if ($adapter.IndexOf($marker, [System.StringComparison]::Ordinal) -lt 0) { throw "Install platform evidence marker missing: $marker" }
}
Write-Output 'WP6_UPDATE_API_ROUTES_PASS'
