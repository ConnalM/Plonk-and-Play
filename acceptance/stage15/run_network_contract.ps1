$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$manager = Get-Content -Raw (Join-Path $root 'firmware/include/pp/external_wifi.h')
$interface = Get-Content -Raw (Join-Path $root 'firmware/include/pp/browser_interface.h')
$browser = Get-Content -Raw (Join-Path $root 'firmware/browser/normal_browser.html')

foreach ($marker in @(
  'WiFi.persistent(true)', 'WiFi.mode(WIFI_AP_STA)', 'WiFi.begin()',
  'WiFi.setAutoReconnect(false)', 'WiFi.disconnect(false, false)',
  'WiFi.disconnect(false, true)', 'MDNS.begin("plonk-and-play")',
  'scanNetworks(true, true)', 'FactoryApSsid = "Plonk-and-Play"')) {
  if ($manager -notlike "*$marker*") { throw "External Wi-Fi manager marker missing: $marker" }
}
if ($manager -match 'WiFi\.psk|password_') { throw 'Credential material is retained or exposed by the manager.' }
foreach ($route in @('/network/status','/network/scan','/network/connect','/network/disconnect','/network/forget')) {
  if ($interface -notlike "*$route*") { throw "Network route missing: $route" }
}
$connectStart = $interface.IndexOf('static esp_err_t networkConnectRoute')
$disconnectStart = $interface.IndexOf('static esp_err_t networkDisconnectRoute')
$forgetStart = $interface.IndexOf('static esp_err_t networkForgetRoute')
if ($connectStart -lt 0 -or $disconnectStart -lt $connectStart -or $forgetStart -lt $disconnectStart) { throw 'Network mutation route ordering missing.' }
if (-not $interface.Contains('ssid[0] != 0')) { throw 'Network credentials parser does not require an SSID while permitting an empty password for open networks.' }
foreach ($section in @(
  $interface.Substring($connectStart, $disconnectStart - $connectStart),
  $interface.Substring($disconnectStart, $forgetStart - $disconnectStart),
  $interface.Substring($forgetStart, $interface.IndexOf('static esp_err_t health', $forgetStart) - $forgetStart))) {
  if ($section -notlike '*context(client(request))*ClientContext::RaceDirectorSmug*') { throw 'Network mutation lacks the controlling-lease guard.' }
}
foreach ($marker in @('Network','networkPage','/network/status','/network/scan','/network/connect','/network/disconnect','/network/forget','type="password"')) {
  if ($browser -notlike "*$marker*") { throw "Browser Network marker missing: $marker" }
}
if ($browser -match 'networkPassword[^;]{0,200}console\.log|networkPassword[^;]{0,200}diagnostic') { throw 'Browser password is sent to diagnostics/logging.' }
Write-Host 'Stage 15 optional external Wi-Fi contract: PASS'
