param(
  [string]$BaseUrl = 'http://127.0.0.1:9080',
  [string]$EvidenceDirectory = '..\evidence'
)

$ErrorActionPreference = 'Stop'
$evidence = [IO.Path]::GetFullPath($EvidenceDirectory)
New-Item -ItemType Directory -Force $evidence | Out-Null
$jarRoot = Join-Path $env:TEMP ('pp-stage10-http-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $jarRoot | Out-Null
$a = Join-Path $jarRoot 'master-A.cookies'
$b = Join-Path $jarRoot 'viewer-B.cookies'
$c = Join-Path $jarRoot 'viewer-C.cookies'
$d = Join-Path $jarRoot 'lost-master.cookies'

function Invoke-PpHttp([string]$jar, [string]$path, [string]$method = 'GET', [string]$body = '', [string[]]$headers = @()) {
  $response = Join-Path $jarRoot ([guid]::NewGuid().ToString('N') + '.body')
  $args = @('-sS', '--max-time', '8', '-o', $response, '-w', '%{http_code}', '-b', $jar, '-c', $jar, '-X', $method)
  foreach ($header in $headers) { $args += @('-H', $header) }
  if ($body) { $args += @('--data', $body) }
  $args += ($BaseUrl + $path)
  $status = (& curl.exe @args).Trim()
  $content = if (Test-Path $response) { [IO.File]::ReadAllText($response) } else { '' }
  Remove-Item $response -ErrorAction SilentlyContinue
  return [pscustomobject]@{ Status = [int]$status; Body = $content }
}

function Get-Result([string]$jar, [uint32]$correlation) {
  for ($i = 0; $i -lt 30; ++$i) {
    $response = Invoke-PpHttp $jar ("/request-result?correlationId=$correlation")
    if ($response.Status -eq 200) { return $response }
    if ($response.Status -ne 204) { return $response }
    Start-Sleep -Milliseconds 100
  }
  return Invoke-PpHttp $jar ("/request-result?correlationId=$correlation")
}

try {
  # Fresh cookie stores must begin with no retained Master binding.  This is a
  # controlled Stage 10 fixture precondition, never a product reset route.
  $aInitial = Invoke-PpHttp $a '/context'
  $bInitial = Invoke-PpHttp $b '/context'
  $cInitial = Invoke-PpHttp $c '/context'
  $cookiesIssued = (Test-Path $a) -and (Test-Path $b) -and (Test-Path $c) -and
    ((Get-Content $a -Raw) -match 'pp_browser') -and ((Get-Content $b -Raw) -match 'pp_browser') -and
    ((Get-Content $c -Raw) -match 'pp_browser')
  $passiveNoMaster = $aInitial.Body -match '"role":"Spectator"' -and
    $bInitial.Body -match '"role":"Spectator"' -and $cInitial.Body -match '"role":"Spectator"' -and
    $aInitial.Body -match '"hasMaster":false' -and $bInitial.Body -match '"hasMaster":false'
  if (-not ($cookiesIssued -and $passiveNoMaster)) { throw 'Fresh HTTP fixture was not in the required no-Master condition.' }

  $preStart = Invoke-PpHttp $b '/request/start' 'POST' '{"correlationId":201}' @('Content-Type: application/json')
  $preResult = Get-Result $b 201
  $preState = Invoke-PpHttp $b '/state'
  $preDenied = $preStart.Status -eq 202 -and $preResult.Body -match 'REJECTED' -and
    $preResult.Body -match 'START permission denied' -and $preState.Body -match '"lifecycle":"READY"'

  $bootstrap = Invoke-PpHttp $a '/bootstrap' 'POST'
  $aRole = Invoke-PpHttp $a '/context'
  $bRole = Invoke-PpHttp $b '/context'
  $repeatA = Invoke-PpHttp $a '/bootstrap' 'POST'
  $repeatB = Invoke-PpHttp $b '/bootstrap' 'POST'
  $oneTimeBootstrap = $bootstrap.Status -eq 200 -and $bootstrap.Body -match 'Race Director' -and
    $aRole.Body -match 'Race Director' -and $bRole.Body -match 'Spectator' -and
    $repeatA.Status -eq 409 -and $repeatB.Status -eq 409

  $claimed = Invoke-PpHttp $b '/request/start?role=RaceDirector&master=true&identity=master-A' 'POST' '{"correlationId":202,"role":"RaceDirectorSmug","authority":"Master","identity":"master-A"}' @('Content-Type: application/json', 'X-Role: RaceDirectorSmug', 'X-Master: true', 'X-Browser-Identity: master-A')
  $claimedResult = Get-Result $b 202
  $claimsDenied = $claimed.Status -eq 202 -and $claimedResult.Body -match 'REJECTED' -and
    $claimedResult.Body -match 'START permission denied' -and (Invoke-PpHttp $b '/context').Body -match 'Spectator'

  $masterStart = Invoke-PpHttp $a '/request/start' 'POST' '{"correlationId":300}' @('Content-Type: application/json')
  $masterResult = Get-Result $a 300
  Start-Sleep -Milliseconds 1100
  $stateAfterMaster = Invoke-PpHttp $a '/state'
  $masterStartPass = $masterStart.Status -eq 202 -and $masterResult.Body -match 'ACCEPTED' -and
    $stateAfterMaster.Body -match '"lifecycle":"RACING"' -and $stateAfterMaster.Body -match '"entries"'

  $spectatorStart = Invoke-PpHttp $b '/request/start?role=RaceDirector' 'POST' '{"correlationId":300,"role":"RaceDirectorSmug"}' @('Content-Type: application/json', 'X-Role: RaceDirectorSmug')
  $spectatorResult = Get-Result $b 300
  $stateAfterSpectator = Invoke-PpHttp $b '/state'
  $spectatorDenied = $spectatorStart.Status -eq 202 -and $spectatorResult.Body -match 'REJECTED' -and
    $spectatorResult.Body -match 'START permission denied' -and $stateAfterSpectator.Body -eq $stateAfterMaster.Body

  $crossResult = Invoke-PpHttp $c '/request-result?correlationId=300'
  $privateResults = $masterResult.Body -match 'ACCEPTED' -and $spectatorResult.Body -match 'REJECTED' -and
    $crossResult.Status -eq 204

  $bState = Invoke-PpHttp $b '/state'
  $cState = Invoke-PpHttp $c '/state'
  $separateClients = $bState.Status -eq 200 -and $cState.Status -eq 200 -and
    $bState.Body -match '"entries"' -and $cState.Body -match '"entries"' -and
    (Invoke-PpHttp $b '/context').Body -match 'Spectator' -and (Invoke-PpHttp $c '/context').Body -match 'Spectator'
  $stateReconstruction = $bState.Body -eq $stateAfterMaster.Body -and $cState.Body -eq $stateAfterMaster.Body

  # Same cookie store represents normal reload/reopen.  A new cookie store
  # represents loss of the Master Browser identity and must remain Spectator.
  $aReload = Invoke-PpHttp $a '/context'
  $freshLost = Invoke-PpHttp $d '/context'
  $cookieRetained = $aReload.Body -match 'Race Director'
  $retainedAuthority = $bRole.Body -match 'Spectator' -and $aReload.Body -match 'Race Director'
  $lostIdentityNoPromotion = $freshLost.Body -match 'Spectator' -and $freshLost.Body -match '"hasMaster":true'

  $results = [ordered]@{
    '10.1' = @{ passive_no_master = $passiveNoMaster; opaque_cookies_issued = $cookiesIssued; prebootstrap_start_denied = $preDenied }
    '10.2' = @{ one_time_bootstrap = $oneTimeBootstrap }
    '10.3' = @{ claims_denied = $claimsDenied }
    '10.4' = @{ master_start = $masterStartPass }
    '10.5' = @{ spectator_denied = $spectatorDenied }
    '10.6' = @{ private_results = $privateResults }
    '10.7' = @{ separate_clients = $separateClients }
    '10.8' = @{ state_reconstruction = $stateReconstruction }
    '10.11' = @{ cookie_retained = $cookieRetained }
    '10.12' = @{ retained_authority = $retainedAuthority }
    '10.13' = @{ lost_identity_no_promotion = $lostIdentityNoPromotion }
    '10.14' = @{ state_reconstruction = $stateReconstruction }
  }
  $results | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $evidence 'http-results.json')
  $safe = [ordered]@{
    endpoint = $BaseUrl
    a_initial = $aInitial.Body
    b_initial = $bInitial.Body
    c_initial = $cInitial.Body
    bootstrap_status = $bootstrap.Status
    claimed_submission = $claimed.Body
    claimed_result = $claimedResult.Body
    viewer_context_after_claim = (Invoke-PpHttp $b '/context').Body
    repeat_statuses = @($repeatA.Status, $repeatB.Status)
    master_result = $masterResult.Body
    spectator_result = $spectatorResult.Body
    third_owner_result_status = $crossResult.Status
    state_after_master = $stateAfterMaster.Body
    all = $results
  }
  $safe | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $evidence 'http-raw-evidence.json')
  $failures = @($results.Values | ForEach-Object { $_.Values } | Where-Object { -not $_ })
  if ($failures.Count -ne 0) { throw 'One or more Stage 10 HTTP acceptance assertions failed.' }
  'PASS'
} finally {
  Remove-Item $jarRoot -Recurse -Force -ErrorAction SilentlyContinue
}
