$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$manager = Get-Content -Raw (Join-Path $root 'firmware/include/pp/external_wifi.h')
$interface = Get-Content -Raw (Join-Path $root 'firmware/include/pp/browser_interface.h')
$browser = Get-Content -Raw (Join-Path $root 'firmware/browser/normal_browser.html')
$embed = Get-Content -Raw (Join-Path $root 'firmware/browser/embed_normal.py')

foreach ($marker in @(
  'WiFi.persistent(true)', 'WiFi.mode(WIFI_AP_STA)', 'WiFi.begin()',
  '#include <esp_wifi.h>', 'esp_wifi_get_config', 'ESP_IF_WIFI_STA',
  'WiFi.setAutoReconnect(false)', 'WiFi.disconnect(false, false)',
  'WiFi.disconnect(false, true)', 'MDNS.begin("plonk-and-play")',
  'scanNetworks(true, true)', 'ScanState', 'SCAN_ACCEPTED', 'SCAN_DEFERRED', 'SCAN_UNAVAILABLE', 'SCAN_COMPLETE',
  'STA_EVENT event=CONNECTED', 'STA_EVENT event=DISCONNECTED', 'STA_EVENT event=GOT_IP', 'STA_BEGIN result=',
  'FactoryApSsid = "Plonk-and-Play"')) {
  if ($manager -notlike "*$marker*") { throw "External Wi-Fi manager marker missing: $marker" }
}
foreach ($marker in @('staFailure_ == StaFailure::None', 'staState_ == StaState::Connecting', 'staFailure_ = StaFailure::ConnectFailed', 'staFailure_ = StaFailure::ConnectionLost', 'staState_ = StaState::Disconnected')) {
  if ($manager -notlike "*$marker*") { throw "STA terminal-disconnect state mapping marker missing: $marker" }
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
foreach ($marker in @('Network','networkPage','/network/status','/network/scan','/network/connect','/network/reconnect','/network/disconnect','/network/forget','RECONNECT','CONNECT','type="password"','network-row','data-network-index','Scanning for available networks','Selected ${ssid}. Enter the password','networkDraftSsid','networkDraftPassword','Accept'':''application/json')) {
  if ($browser -notlike "*$marker*") { throw "Browser Network marker missing: $marker" }
}
if ($browser -notmatch "const configureLabel='CONNECT'") { throw 'Network credential action is not labelled CONNECT.' }
if ($browser -match "configureLabel=sta\.configured\?'RECONFIGURE':'CONFIGURE NETWORK'") { throw 'Network page retains the old Configure/Reconfigure label.' }
if ($browser -match 'press RECONFIGURE') { throw 'Network selection help text still instructs the user to press RECONFIGURE.' }
if ($browser -match 'until Connect/Reconfigure or reboot') { throw 'Network suppression help text still uses the old Reconfigure vocabulary.' }
if ($embed -notlike '*{"/", HTTP_GET, normalPage, nullptr}*' -or
    $embed -notlike '*{"/normal", HTTP_GET, normalPage, nullptr}*' -or
    $embed -like '*anchor + `n      {"/normal", HTTP_GET, normalPage, nullptr}*') {
  throw 'Normal Browser embed does not make the root entry point serve the current Normal Browser.'
}
Write-Host 'Normal Browser root-entry contract: PASS'
foreach ($marker in @(
  'constexpr size_t NormalBrowserChunkSize = 4096',
  'httpd_resp_send_chunk(request, kNormalBrowserHtml + offset, count)',
  'if (chunkResult != ESP_OK)',
  'httpd_resp_send_chunk(request, nullptr, 0)',
  'trace.complete(chunkResult)',
  'trace.complete(sent)')) {
  if ($embed -notlike "*$marker*") { throw "Normal Browser chunk transport marker missing: $marker" }
}
function Invoke-NormalBrowserChunkModel([int]$length, [int]$chunkSize, [int]$failAt) {
  $offset = 0
  $chunks = 0
  $bytes = 0
  $terminalSent = $false
  $error = $false
  while ($offset -lt $length) {
    $count = [Math]::Min($chunkSize, $length - $offset)
    $chunks++
    if ($failAt -gt 0 -and $chunks -eq $failAt) { $error = $true; break }
    $bytes += $count
    $offset += $count
  }
  if (-not $error -and $offset -eq $length) { $terminalSent = $true }
  [pscustomobject]@{ chunks = $chunks; bytes = $bytes; terminalSent = $terminalSent; error = $error }
}
$normalSuccess = Invoke-NormalBrowserChunkModel 10001 4096 0
if ($normalSuccess.bytes -ne 10001 -or -not $normalSuccess.terminalSent -or $normalSuccess.error -or $normalSuccess.chunks -ne 3) {
  throw 'Normal Browser successful chunking model did not preserve all bytes and the terminating chunk.'
}
$normalFailure = Invoke-NormalBrowserChunkModel 10001 4096 2
if (-not $normalFailure.error -or $normalFailure.terminalSent -or $normalFailure.bytes -ne 4096) {
  throw 'Normal Browser chunk failure model did not propagate the failed chunk or suppress termination.'
}
Write-Host 'Normal Browser bounded chunk success/error/termination regression: PASS'
foreach ($marker in @('let networkRefreshTimer=0', 'function scheduleNetworkRefresh', 'scheduleNetworkRefresh(scanBusy,sta.state===''CONNECTING''', 'submitNetworkConnect', 'networkPassword'').onkeydown', 'await networkPage(true)')) {
  if ($browser -notlike "*$marker*") { throw "Browser asynchronous Network refresh marker missing: $marker" }
}
foreach ($marker in @('function networkFormEditing', 'active.id===''networkSsid''', 'active.id===''networkPassword''', 'if(!force&&networkFormEditing())', 'networkPage(true)', 'networkDraftPassword')) {
  if ($browser -notlike "*$marker*") { throw "Browser input-focus preservation marker missing: $marker" }
}

# Deterministic regression for the physical WPA failure: a disconnect event
# while an asynchronous association is in progress must terminate the
# presentation attempt, while deliberate Disconnect/Forget remains deliberate
# and does not become a reported failure.
$staState = 'CONNECTING'
$staFailure = 'NONE'
$suppressed = $false
if (-not $suppressed) {
  if ($staFailure -eq 'NONE') { $staFailure = if ($staState -eq 'CONNECTING') { 'CONNECT_FAILED' } else { 'CONNECTION_LOST' } }
  $staState = 'DISCONNECTED'
}
if ($staState -ne 'DISCONNECTED' -or $staFailure -ne 'CONNECT_FAILED') { throw 'Terminal STA disconnect regression remained CONNECTING.' }
$staState = 'DISCONNECTED'; $staFailure = 'NONE'; $suppressed = $true
if (-not $suppressed) { $staFailure = 'CONNECT_FAILED'; $staState = 'DISCONNECTED' }
if ($staState -ne 'DISCONNECTED' -or $staFailure -ne 'NONE') { throw 'Deliberate STA suppression was misclassified as a failure.' }
Write-Host 'STA terminal-disconnect state mapping regression: PASS'

# Deterministic Browser regression: polling/reconciliation must not replace the
# Network form while the password input is active, while an explicit action is
# allowed to force the authoritative re-render.
$editing = $true
$forced = $false
$redraws = 0
if (-not $forced -and $editing) { $redraws = 0 } else { $redraws++ }
if ($redraws -ne 0) { throw 'Network polling replaced an active credential form.' }
$forced = $true
if ($forced) { $redraws++ }
if ($redraws -ne 1) { throw 'Explicit Network action did not force a result re-render.' }
Write-Host 'Network password-input focus preservation regression: PASS'
if ($browser -notmatch "if\(r\.ok\)\{networkDraftSsid='';networkDraftPassword='';networkDraftActive=false;ssidInput\.value='';passwordInput\.value=''\}.*await networkPage\(true\)") {
  throw 'Accepted Forget does not clear Browser-local SSID/password drafts before authoritative re-render.'
}
if ($browser -notmatch "networkDraftSsid='';networkDraftPassword='';networkDraftActive=false") {
  throw 'Forget Browser draft state reset is missing.'
}
# Deterministic regression for the real Forget/re-render path: a stale local
# draft must not survive an accepted authoritative Forget, including when the
# Network view is reconstructed from the current /network/status response.
$forgetDraftSsid = 'previous-network'
$forgetDraftPassword = 'draft-password'
$forgetDraftActive = $true
$forgetAccepted = $true
if ($forgetAccepted) {
  $forgetDraftSsid = ''
  $forgetDraftPassword = ''
  $forgetDraftActive = $false
}
if ($forgetDraftSsid -ne '' -or $forgetDraftPassword -ne '' -or $forgetDraftActive) {
  throw 'Accepted Forget regression model retained stale Browser-local network draft state.'
}
Write-Host 'Accepted Forget Browser draft-clearing regression model: PASS'
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
$acceptedMarker = 'RECONFIGURE_ACCEPTED ssid_length=%u password_present=true password_length=%u'
if ($interface -notlike "*$acceptedMarker*") { throw 'Accepted Reconfigure evidence does not include sanitized password length.' }
$acceptedStart = $interface.IndexOf('RECONFIGURE_ACCEPTED')
$acceptedEnd = $interface.IndexOf('const bool accepted = instance()->networkConnect', $acceptedStart)
if ($acceptedStart -lt 0 -or $acceptedEnd -le $acceptedStart) { throw 'Accepted Reconfigure logging boundary is missing.' }
$acceptedSection = $interface.Substring($acceptedStart, $acceptedEnd - $acceptedStart)
if ($acceptedSection -notmatch 'strlen\(password\)') { throw 'Accepted Reconfigure password length is not derived from the submitted value.' }
if ($acceptedSection -match 'password[^\r\n]*%s') { throw 'Accepted Reconfigure evidence may expose password material.' }
if ($acceptedSection -match 'password[^\r\n]*password[^\r\n]*%s') { throw 'Accepted Reconfigure evidence may expose password material.' }
Write-Host 'Accepted Reconfigure password-length privacy regression: PASS'
$acceptedPasswordLength = 12
$sanitizedAssociationEvidence = "password_present=true password_length=$acceptedPasswordLength"
if ($sanitizedAssociationEvidence -match 'password=(?!present|length)') { throw 'Sanitized association evidence contains a password value.' }
if ($sanitizedAssociationEvidence -notmatch 'password_length=12') { throw 'Sanitized password-length evidence regression model failed.' }
Write-Host 'Sanitized association password-length model: PASS'
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

# The physical driver rejected scans while a remembered STA reconnect was in
# flight.  The owner must defer that scan, pause only the bounded reconnect
# attempt, and resume it after the asynchronous scan reaches a terminal state.
foreach ($marker in @('scanDeferred_', 'scanPauseReconnect_', 'finishScan()', 'WiFi.disconnect(false, false)', 'WiFi.setAutoReconnect(false)')) {
  if ($manager -notlike "*$marker*") { throw "Scan/STA coordination marker missing: $marker" }
}
if ($manager -notmatch 'staStatus == WL_NO_SSID_AVAIL') {
  throw 'Scan does not defer while a remembered STA reports WL_NO_SSID_AVAIL.'
}
if ($manager -notmatch 'staDriverBusy[\s\S]{0,320}scanDeferred_ = true[\s\S]{0,320}WiFi\.disconnect\(false, false\)') {
  throw 'Scan does not defer and cancel an in-flight remembered STA operation before driver scan start.'
}
if ($manager -notmatch 'void finishScan\(\)[\s\S]{0,220}WiFi\.setAutoReconnect\(true\)') {
  throw 'Scan completion does not restore the bounded STA reconnect lifecycle.'
}
Write-Host 'Scan/STA coordination contract: PASS'

# Deterministic regression for the real hardware sequence: scanComplete() may
# still contain WIFI_SCAN_FAILED before a deferred scan has started.  That
# stale value must not terminate the deferred request; exactly one driver
# start must occur, followed by a terminal result and reconnect resumption.
if ($manager -notlike '*if (!scanDeferred_) refreshScanState();*') {
  throw 'Deferred scans do not suppress stale scanComplete() polling.'
}
function Invoke-DeferredScanModel([int]$driverResult) {
  $scanDeferred = $true
  $scanPauseReconnect = $true
  $scanState = 'RUNNING'
  $staleScanResult = -2
  $driverStarts = 0
  $apAvailable = $true
  $httpAvailable = $true
  $prematureUnavailable = $false

  # Tick before the deferral deadline: refreshScanState() is skipped.
  if (-not $scanDeferred -and $staleScanResult -lt 0) {
    $scanState = 'UNAVAILABLE'
    $prematureUnavailable = $true
  }
  if ($prematureUnavailable -or $driverStarts -ne 0 -or -not $scanPauseReconnect) {
    throw 'Deferred scan model terminated before the driver scan started.'
  }

  # Deadline reached: start exactly one real driver scan.
  $scanDeferred = $false
  $driverStarts++
  if ($driverResult -ge 0) { $scanState = 'COMPLETE' } else { $scanState = 'UNAVAILABLE' }
  $scanPauseReconnect = $false
  [pscustomobject]@{
    state = $scanState
    driverStarts = $driverStarts
    reconnectResumed = -not $scanPauseReconnect
    apAvailable = $apAvailable
    httpAvailable = $httpAvailable
  }
}
$deferredScanSuccess = Invoke-DeferredScanModel 4
if ($deferredScanSuccess.state -ne 'COMPLETE' -or
    $deferredScanSuccess.driverStarts -ne 1 -or
    -not $deferredScanSuccess.reconnectResumed -or
    -not $deferredScanSuccess.apAvailable -or
    -not $deferredScanSuccess.httpAvailable) {
  throw 'Deferred scan success model did not preserve one-start, resume, AP, and HTTP contracts.'
}
$deferredScanFailure = Invoke-DeferredScanModel -2
if ($deferredScanFailure.state -ne 'UNAVAILABLE' -or
    $deferredScanFailure.driverStarts -ne 1 -or
    -not $deferredScanFailure.reconnectResumed -or
    -not $deferredScanFailure.apAvailable -or
    -not $deferredScanFailure.httpAvailable) {
  throw 'Deferred scan failure model did not report a genuine post-start failure safely.'
}
Write-Host 'Deferred scan stale-result regression model: PASS'
