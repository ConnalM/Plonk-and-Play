$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent | Split-Path -Parent
$firmware=Join-Path $root 'firmware'
$evidence=Join-Path $PSScriptRoot 'evidence'
New-Item -ItemType Directory -Force $evidence | Out-Null
$cli=Join-Path $env:USERPROFILE '.wokwi/bin/wokwi-cli.exe'
$token=Join-Path $env:USERPROFILE '.wokwi/cli-token.clixml'
$old=[Environment]::GetEnvironmentVariable('WOKWI_CLI_TOKEN','Process');$secret=$null
try {
  if(-not $old){$secret=Import-Clixml $token;$env:WOKWI_CLI_TOKEN=[Net.NetworkCredential]::new('',$secret).Password}
  & $cli $firmware --elf (Join-Path $firmware '.pio/build/stage11acceptance/firmware.elf') --timeout 90000 --scenario (Join-Path $PSScriptRoot 'scenario.yaml') --serial-log-file (Join-Path $evidence 'serial.txt') *> (Join-Path $evidence 'acceptance-cli.txt')
  if($LASTEXITCODE -ne 0){throw 'Stage 11 Wokwi simulation failed.'}
} finally {[Environment]::SetEnvironmentVariable('WOKWI_CLI_TOKEN',$old,'Process');if($secret){$secret.Dispose()}}
