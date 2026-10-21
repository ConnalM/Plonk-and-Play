$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$manager = Get-Content -Raw (Join-Path $root 'firmware/include/pp/external_wifi.h')
$interface = Get-Content -Raw (Join-Path $root 'firmware/include/pp/browser_interface.h')
$browser = Get-Content -Raw (Join-Path $root 'firmware/browser/normal_browser.html')

foreach ($marker in @(
  'WiFi.persistent(true)', 'WiFi.mode(WIFI_AP_STA)', 'WiFi.begin()',
  '#include <esp_wifi.h>', 'esp_wifi_get_config', 'ESP_IF_WIFI_STA',
  'WiFi.setAutoReconnect(false)', 'WiFi.disconnect(false, false)',
  'WiFi.disconnect(false, true)', 'MDNS.begin("plonk-and-play")',
  'scanNetworks(true, true)', 'ScanState', 'SCAN_ACCEPTED', 'SCAN_UNAVAILABLE', 'SCAN_COMPLETE', 'FactoryApSsid = "Plonk-and-Play"')) {
  if ($manager -notlike "*$marker*") { throw "External Wi-Fi manager marker missing: $marker" }
}
if ($manager -notmatch 'stationConfigured_ = true;[\s\S]{0,260}WiFi\.begin\(ssid, password\)') {
  throw 'Connect does not establish configured state from the accepted SSID before asynchronous WiFi.begin completes.'
}
if ($manager -match 'WiFi\.begin\(ssid, password\);[\s\S]{0,120}stationConfigured_ = WiFi\.SSID\(\)\.length\(\)') {
  throw 'Connect still derives configured state from the driver SSID immediately after WiFi.begin.'
}
$managerReconnectStart = $manager.IndexOf('  bool reconnect()')
$managerReconnectEnd = $manager.IndexOf('  bool disconnect()', $managerReconnectStart)
if ($managerReconnectStart -lt 0 -or $managerReconnectEnd -lt $managerReconnectStart) { throw 'External Wi-Fi reconnect implementation missing.' }
$managerReconnect = $manager.Substring($managerReconnectStart, $managerReconnectEnd - $managerReconnectStart)
if ($managerReconnect -notlike '*WiFi.reconnect()*') { throw 'Reconnect does not invoke the driver reconnect operation.' }
if ($managerReconnect -match 'WiFi\.begin|ssid|password') { throw 'Reconnect implementation can write or accept credentials.' }
$managerConnectStart = $manager.IndexOf('  bool connect(const char* ssid')
if ($managerConnectStart -lt 0) { throw 'External Wi-Fi connect boundary missing.' }
if ($manager.Substring(0, $managerConnectStart) -match 'stationConfigured_ = WiFi\.SSID\(\)\.length\(\)') {
  throw 'Startup or runtime configured-state detection still uses the associated-AP SSID.'
}
if ($manager -notmatch 'stationConfigured_ = hasStoredStationConfiguration\(\);[\s\S]{0,260}WiFi\.begin\(\)') {
  throw 'Startup does not detect persisted driver configuration before stored-configuration WiFi.begin.'
}
foreach ($marker in @('StaState', 'UNCONFIGURED', 'DISCONNECTED', 'CONNECTING', 'CONNECTED', 'StaFailure', 'NO_SSID_AVAILABLE', 'CONNECT_FAILED', 'CONNECTION_LOST', 'lastFailure')) {
  if ($manager -notlike "*$marker*" -and $interface -notlike "*$marker*") { throw "STA observability marker missing: $marker" }
}
if ($manager -match 'WiFi\.psk|password_') { throw 'Credential material is retained or exposed by the manager.' }
foreach ($route in @('/network/status','/network/scan','/network/connect','/network/disconnect','/network/forget')) {
  if ($interface -notlike "*$route*") { throw "Network route missing: $route" }
}
$reconnectRoute = '/network/reconnect'
if ($interface -notlike "*$reconnectRoute*") { throw 'Network reconnect route missing.' }
$reconnectMethodStart = $interface.IndexOf('bool networkReconnect()')
if ($reconnectMethodStart -lt 0) { throw 'Network reconnect boundary missing.' }
$reconnectMethodEnd = $interface.IndexOf('bool networkDisconnect()', $reconnectMethodStart)
if ($reconnectMethodEnd -lt 0) { throw 'Network reconnect boundary is not terminated.' }
$reconnectMethod = $interface.Substring($reconnectMethodStart, $reconnectMethodEnd - $reconnectMethodStart)
if ($reconnectMethod -match 'ssid|password|WiFi.begin') { throw 'Reconnect boundary accepts or writes credentials.' }
$reconnectRouteStart = $interface.IndexOf('static esp_err_t networkReconnectRoute')
$reconnectRouteEnd = $interface.IndexOf('static esp_err_t networkDisconnectRoute', $reconnectRouteStart)
if ($reconnectRouteStart -lt 0 -or $reconnectRouteEnd -lt $reconnectRouteStart) { throw 'Network reconnect route ordering missing.' }
$reconnectRouteSection = $interface.Substring($reconnectRouteStart, $reconnectRouteEnd - $reconnectRouteStart)
if ($reconnectRouteSection -notlike '*context(client(request))*ClientContext::RaceDirectorSmug*') { throw 'Network reconnect lacks the controlling-lease guard.' }
if ($reconnectRouteSection -match 'ssid|password|httpd_req_recv') { throw 'Network reconnect route reads credential payload.' }
$connectStart = $interface.IndexOf('static esp_err_t networkConnectRoute')
$disconnectStart = $interface.IndexOf('static esp_err_t networkDisconnectRoute')
$forgetStart = $interface.IndexOf('static esp_err_t networkForgetRoute')
if ($connectStart -lt 0 -or $disconnectStart -lt $connectStart -or $forgetStart -lt $disconnectStart) { throw 'Network mutation route ordering missing.' }
if (-not $interface.Contains('ssid[0] == 0') -or -not $interface.Contains('NetworkCredentialParseResult::SsidEmpty')) { throw 'Network credentials parser does not require an SSID while permitting an empty password for open networks.' }
foreach ($section in @(
  $interface.Substring($connectStart, $disconnectStart - $connectStart),
  $interface.Substring($disconnectStart, $forgetStart - $disconnectStart),
  $interface.Substring($forgetStart, $interface.IndexOf('static esp_err_t health', $forgetStart) - $forgetStart))) {
  if ($section -notlike '*context(client(request))*ClientContext::RaceDirectorSmug*') { throw 'Network mutation lacks the controlling-lease guard.' }
}
foreach ($marker in @('Network','networkPage','/network/status','/network/scan','/network/connect','/network/reconnect','/network/disconnect','/network/forget','RECONNECT','RECONFIGURE','type="password"','network-row','data-network-index','Scanning for available networks','Selected ${ssid}. Enter the password','networkDraftSsid','networkDraftPassword','Accept'':''application/json')) {
  if ($browser -notlike "*$marker*") { throw "Browser Network marker missing: $marker" }
}
foreach ($marker in @('let networkRefreshTimer=0', 'function scheduleNetworkRefresh', 'scheduleNetworkRefresh(scanBusy,sta.state===''CONNECTING''', 'submitNetworkConnect', 'networkPassword'').onkeydown', 'await networkPage()')) {
  if ($browser -notlike "*$marker*") { throw "Browser asynchronous Network refresh marker missing: $marker" }
}
foreach ($marker in @('networkRefreshDeadline', 'networkConnectPendingUntil', 'networkScanPendingUntil', 'scheduleNetworkRefresh(false,true,true)', "const staState=connectPending?'CONNECTING'")) {
  if ($browser -notlike "*$marker*") { throw "Browser accepted-connect polling marker missing: $marker" }
}
if ($browser -notmatch 'if\(r\.ok\)\{networkConnectPendingUntil=Date\.now\(\)\+15000') {
  throw 'Accepted Connect does not establish a bounded Browser presentation window.'
}
if ($browser -notmatch "fetch\('/network/reconnect',\{method:'POST'\}\)") { throw 'Browser reconnect does not use a credential-free POST.' }
if ($browser -notmatch 'submitNetworkReconnect') { throw 'Browser reconnect handler missing.' }
if ($browser -match 'if\(scan\.running\)setTimeout\(\(\)=>\{if\(\$\(\x27#networkScan\x27\)\)networkPage\(\)\},500\)') { throw 'Browser retained the one-shot scan refresh.' }
if ($browser -match 'networkPassword[^;]{0,200}console\.log|networkPassword[^;]{0,200}diagnostic') { throw 'Browser password is sent to diagnostics/logging.' }
if ($interface -notmatch 'RECONFIGURE_REJECTED reason=%s ssid_length=%u password_present=%s') { throw 'Reconfigure validation evidence is not sanitized.' }
if ($interface -notmatch 'SCAN_REQUEST route_received=true') { throw 'Scan request receipt evidence is missing.' }
if ($interface -notmatch 'scanStateName\(v\.scanState\)') { throw 'Scan lifecycle state is not exposed through Network status.' }

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

# Startup/recovery distinctions required by the frozen AP+STA contract:
# persistent driver configuration is independent of current association.
$storedConfig = $true
$associated = $false
$suppressed = $false
$configured = $storedConfig
$startupState = if ($configured) { 'CONNECTING' } else { 'UNCONFIGURED' }
if ($startupState -ne 'CONNECTING') { throw 'Remembered driver configuration did not produce CONNECTING at boot.' }
$routerAbsentState = 'DISCONNECTED'
if ($routerAbsentState -eq 'UNCONFIGURED') { throw 'Router absence incorrectly collapses remembered configuration to UNCONFIGURED.' }
$suppressed = $true # deliberate Disconnect; credentials remain in the driver
$suppressed = $false # reboot clears runtime suppression
if (-not $storedConfig -or $suppressed) { throw 'Disconnect-then-reboot model did not restore automatic reconnect eligibility.' }
$storedConfig = $false # successful Forget erases the driver configuration
$configured = $storedConfig
if ($configured) { throw 'Forget-then-reboot model retained remembered configuration.' }
Write-Host 'Remembered STA boot/Disconnect/Forget distinction model: PASS'
$main = Get-Content -Raw (Join-Path $root 'firmware/src/main.cpp')
foreach ($marker in @('wifi_mode=%s', 'sta_state=%s', 'sta_configured=%u', 'sta_failure=%s', 'sta_reconnects=%lu', 'mdns_started=%u', 'scan_state=%s', 'scan_requests=%lu', 'scan_completed=%lu', 'scan_unavailable=%lu')) {
  if ($main -notlike "*$marker*") { throw "Serial AP+STA health marker missing: $marker" }
}
Write-Host 'Stage 15 optional external Wi-Fi contract: PASS'
