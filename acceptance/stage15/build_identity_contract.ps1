param(
  [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\..\firmware\.pio\build\stage8bp1s3sd'),
  [string]$ExpectedCommit = '',
  [string]$BinaryPath = ''
)

$ErrorActionPreference = 'Stop'
$header = Join-Path $BuildDirectory 'generated\pp\build_identity_generated.h'
$binary = if ($BinaryPath) { $BinaryPath } else { Join-Path $BuildDirectory 'firmware.merged.instrumented.bin' }
if (-not (Test-Path $header)) { throw "Generated build identity header missing: $header" }
if (-not (Test-Path $binary)) { throw "Merged candidate missing: $binary" }

$text = Get-Content -Raw $header
$commitMatch = [regex]::Match($text, '#define PP_BUILD_SOURCE_COMMIT "([0-9a-f]+)"')
$environmentMatch = [regex]::Match($text, '#define PP_BUILD_ENVIRONMENT "([^"]+)"')
if (-not $commitMatch.Success -or $commitMatch.Groups[1].Value -eq 'unknown') {
  throw 'Generated source commit is missing or unknown.'
}
if (-not $environmentMatch.Success -or $environmentMatch.Groups[1].Value -ne 'stage8bp1s3sd') {
  throw 'Generated build environment is missing or incorrect.'
}
if ($ExpectedCommit -and $commitMatch.Groups[1].Value -ne $ExpectedCommit) {
  throw "Generated commit $($commitMatch.Groups[1].Value) does not match $ExpectedCommit."
}

$source = Get-Content -Raw (Join-Path $PSScriptRoot '..\..\firmware\src\main.cpp')
$browser = Get-Content -Raw (Join-Path $PSScriptRoot '..\..\firmware\include\pp\browser_interface.h')
$export = Get-Content -Raw (Join-Path $PSScriptRoot '..\..\firmware\include\pp\diagnostic_export.h')
foreach ($marker in @('esp_ota_get_app_elf_sha256', 'setDiagnosticBuildProvenance',
                      'sourceCommit', 'buildEnvironment', 'appElfSha256')) {
  if ($source.IndexOf($marker) -lt 0 -and $browser.IndexOf($marker) -lt 0 -and $export.IndexOf($marker) -lt 0) {
    throw "Build identity marker missing: $marker"
  }
}

# The source commit and environment are deliberately embedded as strings in
# the running image; this catches stale-image selection before a flash.
$bytes = [IO.File]::ReadAllBytes($binary)
$ascii = [Text.Encoding]::ASCII.GetString($bytes)
if ($ascii.IndexOf($commitMatch.Groups[1].Value) -lt 0 -or
    $ascii.IndexOf($environmentMatch.Groups[1].Value) -lt 0) {
  throw 'Merged image does not contain the generated source commit/environment.'
}
Write-Host "Build identity contract PASS: commit=$($commitMatch.Groups[1].Value) environment=$($environmentMatch.Groups[1].Value)"
