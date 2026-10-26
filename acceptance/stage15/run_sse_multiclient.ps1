param([string]$BaseUrl = $env:PP_MULTI_CLIENT_BASE_URL)
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$browser = Get-Content -Raw (Join-Path $root 'firmware/include/pp/browser_interface.h')
$platformio = Get-Content -Raw (Join-Path $root 'firmware/platformio.ini')
$main = Get-Content -Raw (Join-Path $root 'firmware/src/main.cpp')

function Require([string]$text, [string]$pattern, [string]$message) {
  if ($text -notmatch $pattern) { throw $message }
}

# Production-path structural checks: the implementation must have a bounded,
# per-client session identity and must validate work against that identity.
Require $browser '#define PP_MAX_SSE_CLIENTS 4' 'missing provisional SSE capacity default'
Require $platformio '-DPP_MAX_SSE_CLIENTS=4' 'P1 does not select the provisional SSE capacity'
Require $browser 'SseSession\* sseSessions_\[MaxSseClients\]' 'missing bounded session table'
Require $browser 'uint64_t clientToken[^;]*;[\s\S]{0,180}uint32_t generation[^;]*;' 'missing client/generation identity'
Require $browser 'httpd_sess_set_ctx\([^;]+freeSseSession' 'missing HTTP session cleanup association'
Require $browser 'session->generation' 'missing generation identity'
Require $browser 'findSseSessionExact\(session\)' 'queued work is not session-identity checked'
Require $browser 'httpd_req_async_handler_begin' 'missing async request ownership'
Require $browser 'httpd_req_async_handler_complete' 'missing async request release'
Require $browser 'PageWorkerCount = 2' 'missing bounded page worker capacity'
Require $browser 'PP_HTTP_ORDINARY_REQUEST_RESERVE' 'missing ordinary request reservation'
Require $browser 'httpd_get_client_list' 'missing socket-capacity admission check'
if ($browser.Contains('httpd_queue_work(server_, &BrowserInterface::eventWork, work)')) { throw 'SSE still uses HTTPD queue work for sends' }
if ($browser.Contains('httpd_socket_send')) { throw 'SSE still uses raw HTTPD socket sends' }
Require $browser 'httpd_sess_get_ctx\(handle, fd\) != session' 'missing fd/session validation'
Require $browser '503 Service Unavailable' 'missing bounded-capacity response'
Require $browser 'findSseSession\(browserToken\)[\s\S]{0,180}invalidateSseSession\(previous, true, false\)' 'same-identity reconnect does not replace its own generation'
Require $browser 'for \(size_t i = 0; i < MaxSseClients; \+\+i\)' 'missing per-client fan-out iteration'
Require $main 'PP_PHASE8B_P1_TEST' 'P1 fixture guard missing from main path'
Require $browser 'PP_PHASE8B_P1_TEST' 'P1 fixture route guard missing from Browser path'

# Deterministic acceptance model for the bounded identity rules.  This does
# not replace ESP-IDF runtime coverage; it proves the cases independently of
# host HTTP availability and documents the expected table semantics.
$slots = @{}
$nextGeneration = 0
function Connect([UInt64]$token) {
  if ($slots.ContainsKey($token)) { $slots.Remove($token) }
  if ($slots.Count -ge 4) { return $null }
  $script:nextGeneration++
  $entry = [pscustomobject]@{ Token=$token; Generation=$script:nextGeneration; Valid=$true }
  $slots[$token] = $entry
  return $entry
}
function Disconnect([UInt64]$token) { [void]$slots.Remove($token) }

$a = Connect 101; $b = Connect 202; $c = Connect 303; $d = Connect 404
if ($null -eq $a -or $null -eq $b -or $null -eq $c -or $null -eq $d) { throw 'four-client capacity did not admit four clients' }
if ($null -ne (Connect 505)) { throw 'fifth client was admitted beyond bounded capacity' }

$oldGeneration = $a.Generation
$a2 = Connect 101
if ($null -eq $a2 -or $a2.Generation -le $oldGeneration -or $slots.Count -ne 4) { throw 'same-identity reconnect did not replace only its own generation' }
if ($oldGeneration -eq $a2.Generation) { throw 'reconnect reused the old generation' }

# A queued item for the old generation must be stale after replacement.
if ($slots[101].Generation -eq $oldGeneration) { throw 'stale generation remained current' }
Disconnect 202
if (($slots.Count -ne 3) -or (-not ($slots.ContainsKey([UInt64]101))) -or (-not ($slots.ContainsKey([UInt64]303)))) {
  throw "disconnect disturbed another client count=$($slots.Count) keys=$([string]::Join(',', @($slots.Keys)))"
}

Write-Host 'STAGE15 bounded multi-client SSE/P1 fixture contract: PASS'

# The production HTTP/SSE choreography uses the same acceptance boundary as
# the Stage 11–13 HTTP probes.  It is opt-in because starting the controller
# and gateway is environment-specific; absence of a URL is reported as an
# unexecuted runtime portion rather than silently claimed as a pass.
python -m py_compile (Join-Path $PSScriptRoot 'multiclient_http_acceptance.py')
if ($BaseUrl) {
  python (Join-Path $PSScriptRoot 'multiclient_http_acceptance.py') $BaseUrl
} else {
  Write-Host 'STAGE15 production HTTP/SSE choreography: NOT EXECUTED (set PP_MULTI_CLIENT_BASE_URL)'
}
