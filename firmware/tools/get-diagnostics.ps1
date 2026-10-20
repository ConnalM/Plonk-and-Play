param(
  # The normal development route is the P&P factory AP.  An explicit value
  # or PP_CONTROLLER_URL may be used for a controller reached through LAN.
  [string]$BaseUrl = $env:PP_CONTROLLER_URL,
  [int]$TimeoutSeconds = 10
)

$ErrorActionPreference = 'Stop'

$firmwareRoot = Split-Path -Parent $PSScriptRoot
$logRoot = Join-Path $firmwareRoot 'logs'
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null

$explicitAddress = $PSBoundParameters.ContainsKey('BaseUrl') -or
                   -not [string]::IsNullOrWhiteSpace($env:PP_CONTROLLER_URL)
$defaultDiscovery = [string]::IsNullOrWhiteSpace($BaseUrl)
if ($defaultDiscovery) {
  $candidates = @('http://plonk-and-play.local', 'http://192.168.4.1')
} else {
  $candidates = @($BaseUrl.TrimEnd('/'))
}

$timestamp = Get-Date -Format 'yyMMdd-HHmmss-fff'
$artifactPath = Join-Path $logRoot "P-and-P-Diagnostics-$timestamp.txt"
$pointerPath = Join-Path $logRoot 'latest-diagnostics.path.txt'
$attempts = [System.Collections.Generic.List[string]]::new()
$failureKinds = [System.Collections.Generic.List[string]]::new()
[int]$attemptNumber = 0
[void][Reflection.Assembly]::LoadWithPartialName('System.Net.Http')
$client = [System.Net.Http.HttpClient]::new()
$client.Timeout = [TimeSpan]::FromSeconds([Math]::Max(1, $TimeoutSeconds))

try {
  foreach ($candidate in $candidates) {
    $attemptNumber++
    $url = "$($candidate.TrimEnd('/'))/diag/download"

    # mDNS can become available shortly after the controller's AP/STA and
    # HTTP service start.  Probe it with short bounded attempts before using
    # the factory-AP fallback.  Explicit -BaseUrl requests retain the
    # original single-attempt behaviour.
    if ($defaultDiscovery -and $candidate -eq $candidates[0]) {
      $mdnsDeadline = (Get-Date).AddSeconds(8)
      $mdnsReachable = $false
      $mdnsAttempt = 0
      while ((Get-Date) -lt $mdnsDeadline) {
        $mdnsAttempt++
        Write-Output "Discovery: probing mDNS $candidate (attempt $mdnsAttempt)."
        $previousTimeout = $client.Timeout
        try {
          $client.Timeout = [TimeSpan]::FromSeconds(1)
          $probe = $client.GetAsync("$($candidate.TrimEnd('/'))/network/status").GetAwaiter().GetResult()
          $probe.Dispose()
          $mdnsReachable = $true
          Write-Output "Discovery: mDNS route reachable at $candidate."
          break
        } catch {
          $attempts.Add("$candidate/network/status :: $($_.Exception.Message)")
          Start-Sleep -Milliseconds 750
        } finally {
          $client.Timeout = $previousTimeout
        }
      }
      if (-not $mdnsReachable) {
        Write-Output "Discovery: mDNS route unavailable after bounded retry; falling back to $($candidates[1])."
        continue
      }
    } else {
      Write-Output "Discovery: trying controller route $candidate."
    }

    try {
      $response = $client.GetAsync($url).GetAwaiter().GetResult()
      $bytes = $response.Content.ReadAsByteArrayAsync().GetAwaiter().GetResult()
      $status = [int]$response.StatusCode
      $bodyText = [Text.Encoding]::UTF8.GetString($bytes)

      if (-not $response.IsSuccessStatusCode) {
        $kind = if ($status -eq 503 -or $bodyText -match 'diagnostic export unavailable|diagnostics unavailable') {
          'P&P_DIAGNOSTICS_UNAVAILABLE'
        } else {
          'P&P_HTTP_FAILURE'
        }
        $failurePath = Join-Path $logRoot "P-and-P-Diagnostics-$timestamp-error-$attemptNumber.txt"
        [IO.File]::WriteAllBytes($failurePath, $bytes)
        $failureKinds.Add($kind)
        throw "$kind HTTP $status from $url; response saved to $failurePath"
      }

      if ($bytes.Length -eq 0 -or $bodyText -notmatch 'pp-diagnostics-v1') {
        $failurePath = Join-Path $logRoot "P-and-P-Diagnostics-$timestamp-error-$attemptNumber.txt"
        [IO.File]::WriteAllBytes($failurePath, $bytes)
        $failureKinds.Add('P&P_HTTP_FAILURE')
        throw "P&P_HTTP_FAILURE HTTP $status from $url did not contain pp-diagnostics-v1; response saved to $failurePath"
      }

      # Preserve the response bytes exactly as received.  The timestamped
      # artifact is never overwritten; only the pointer below moves forward.
      [IO.File]::WriteAllBytes($artifactPath, $bytes)
      $sha = (Get-FileHash -LiteralPath $artifactPath -Algorithm SHA256).Hash
      @(
        "retrieved_at=$((Get-Date).ToString('o'))"
        "url=$url"
        "artifact=$artifactPath"
        "bytes=$($bytes.Length)"
        "sha256=$sha"
      ) | Set-Content -LiteralPath $pointerPath -Encoding UTF8

      Write-Output "P&P diagnostics retrieved: $artifactPath"
      Write-Output "URL: $url"
      Write-Output "Bytes: $($bytes.Length)"
      Write-Output "SHA-256: $sha"
      exit 0
    } catch {
      $attempts.Add("$url :: $($_.Exception.Message)")
      if ($explicitAddress -or $candidate -eq $candidates[-1]) { break }
    }
  }

  $overallKind = if ($failureKinds -contains 'P&P_DIAGNOSTICS_UNAVAILABLE') {
    'P&P_DIAGNOSTICS_UNAVAILABLE'
  } elseif ($failureKinds.Count -gt 0) {
    'P&P_HTTP_FAILURE'
  } else {
    'CONTROLLER_UNREACHABLE'
  }
  throw ("${overallKind}: unable to retrieve P&P diagnostics. Attempts:`n" + ($attempts -join "`n"))
} finally {
  $client.Dispose()
}
