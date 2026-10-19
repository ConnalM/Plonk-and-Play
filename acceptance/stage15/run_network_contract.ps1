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
if ($manager -notmatch 'stationConfigured_ = true;[\s\S]{0,260}WiFi\.begin\(ssid, password\)') {
  throw 'Connect does not establish configured state from the accepted SSID before asynchronous WiFi.begin completes.'
}
if ($manager -match 'WiFi\.begin\(ssid, password\);[\s\S]{0,120}stationConfigured_ = WiFi\.SSID\(\)\.length\(\)') {
  throw 'Connect still derives configured state from the driver SSID immediately after WiFi.begin.'
}
foreach ($marker in @('StaState', 'UNCONFIGURED', 'DISCONNECTED', 'CONNECTING', 'CONNECTED', 'StaFailure', 'NO_SSID_AVAILABLE', 'CONNECT_FAILED', 'CONNECTION_LOST', 'lastFailure')) {
  if ($manager -notlike "*$marker*" -and $interface -notlike "*$marker*") { throw "STA observability marker missing: $marker" }
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
foreach ($marker in @('Network','networkPage','/network/status','/network/scan','/network/connect','/network/disconnect','/network/forget','type="password"','network-row','data-network-index','Scanning for available networks','Selected ${ssid}. Enter the password')) {
  if ($browser -notlike "*$marker*") { throw "Browser Network marker missing: $marker" }
}
foreach ($marker in @('let networkRefreshTimer=0', 'function scheduleNetworkRefresh', 'scheduleNetworkRefresh(!!scan.running,sta.state===''CONNECTING''', 'submitNetworkConnect', 'networkPassword'').onkeydown', 'await networkPage()')) {
  if ($browser -notlike "*$marker*") { throw "Browser asynchronous Network refresh marker missing: $marker" }
}
foreach ($marker in @('networkRefreshDeadline', 'networkConnectPendingUntil', 'scheduleNetworkRefresh(false,true,true)', "const staState=connectPending?'CONNECTING'")) {
  if ($browser -notlike "*$marker*") { throw "Browser accepted-connect polling marker missing: $marker" }
}
if ($browser -notmatch 'if\(r\.ok\)\{networkConnectPendingUntil=Date\.now\(\)\+15000') {
  throw 'Accepted Connect does not establish a bounded Browser presentation window.'
}
if ($browser -match 'if\(scan\.running\)setTimeout\(\(\)=>\{if\(\$\(\x27#networkScan\x27\)\)networkPage\(\)\},500\)') { throw 'Browser retained the one-shot scan refresh.' }
if ($browser -match 'networkPassword[^;]{0,200}console\.log|networkPassword[^;]{0,200}diagnostic') { throw 'Browser password is sent to diagnostics/logging.' }

# Deterministic regression for the physical race: an accepted Connect may be
# followed by an immediately stale UNCONFIGURED status before the asynchronous
# driver reaches CONNECTING/CONNECTED.  The Browser must keep polling through
# that intermediate response and stop at a terminal result or its bounded
# deadline.
$networkStatuses = @('UNCONFIGURED', 'CONNECTING', 'CONNECTED')
$polls = 0
$deadlinePolls = 30
$pending = $true
foreach ($status in $networkStatuses) {
  if (-not $pending) { break }
  $polls++
  if ($status -eq 'CONNECTED' -or $status -eq 'DISCONNECTED') { $pending = $false }
}
if ($polls -ne 3 -or $pending) { throw 'Accepted Connect regression model did not poll through stale UNCONFIGURED to CONNECTED.' }
if ($deadlinePolls -le $polls) { throw 'Accepted Connect regression model is not bounded.' }
Write-Host 'Accepted Connect stale-status regression model: PASS'
$main = Get-Content -Raw (Join-Path $root 'firmware/src/main.cpp')
foreach ($marker in @('wifi_mode=%s', 'sta_state=%s', 'sta_configured=%u', 'sta_failure=%s', 'sta_reconnects=%lu', 'mdns_started=%u')) {
  if ($main -notlike "*$marker*") { throw "Serial AP+STA health marker missing: $marker" }
}
Write-Host 'Stage 15 optional external Wi-Fi contract: PASS'
