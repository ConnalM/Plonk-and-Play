$ErrorActionPreference = 'Stop'
$firmware = Join-Path $PSScriptRoot '..\..\firmware'
$cli = Join-Path $env:USERPROFILE '.wokwi\bin\wokwi-cli.exe'
$tokenFile = Join-Path $env:USERPROFILE '.wokwi\cli-token.clixml'
$oldToken = [Environment]::GetEnvironmentVariable('WOKWI_CLI_TOKEN', 'Process')
$secret = $null
try {
  if (-not $oldToken) {
    $secret = Import-Clixml -LiteralPath $tokenFile
    $env:WOKWI_CLI_TOKEN = [Net.NetworkCredential]::new('', $secret).Password
  }
  $log = Join-Path $firmware '.pio\wokwi-dev-serial.log'
  # Project-image mode honors firmware/wokwi.toml and therefore loads the
  # assembled bootloader + partition table + app image.  --elf mode omits the
  # partition table in the simulator and cannot exercise OTA slot discovery.
  & $cli $firmware --timeout 30000 --expect-text 'STAGE1_PASS' --serial-log-file $log
  if ($LASTEXITCODE -ne 0) { throw 'Wokwi normal boot failed' }
} finally {
  [Environment]::SetEnvironmentVariable('WOKWI_CLI_TOKEN', $oldToken, 'Process')
  if ($secret) { $secret.Dispose() }
}
if (-not (Test-Path -LiteralPath $log)) { throw "Wokwi serial log missing: $log" }
$text = Get-Content -Raw -LiteralPath $log
if ($text -notmatch '\[WP6 OTA\] adapter (initialized|initialization FAILED)') { throw 'Wokwi OTA adapter initialization marker missing' }
if ($text -notmatch '\[WP6 OTA\] Browser update API declared') { throw 'Wokwi Browser update API marker missing' }
if ($text -notmatch '\[WP6 MAINT\] Browser maintenance API declared') { throw 'Wokwi Browser maintenance API marker missing' }
if ($text -notmatch 'STAGE1_PASS') { throw 'Wokwi normal boot marker missing' }
Write-Host 'WP6_WOKWI_BOOT_ADAPTER_PASS'
if ($text -match 'initialization FAILED no OTA slots') {
  Write-Host 'LIMITATION: Wokwi image did not expose OTA slots; it proves normal boot and adapter discovery attempt only.'
}
Write-Host 'LIMITATION: Wokwi does not emulate ESP-IDF OTA flash writes, bootloader rollback state, or power-loss boundaries; those remain host-fake and later hardware proof.'
