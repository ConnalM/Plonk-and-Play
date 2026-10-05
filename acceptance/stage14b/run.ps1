$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent | Split-Path -Parent
$firmware=Join-Path $root 'firmware';$evidence=Join-Path $PSScriptRoot 'evidence';New-Item -ItemType Directory -Force $evidence|Out-Null
& (Join-Path $firmware 'build.ps1') -Environment stage14bacceptance
$cli=Join-Path $env:USERPROFILE '.wokwi/bin/wokwi-cli.exe';$token=Join-Path $env:USERPROFILE '.wokwi/cli-token.clixml';$old=$env:WOKWI_CLI_TOKEN;$secret=$null
try{if(-not $old){$secret=Import-Clixml $token;$env:WOKWI_CLI_TOKEN=[Net.NetworkCredential]::new('',$secret).Password}; & $cli $firmware --elf (Join-Path $firmware '.pio/build/stage14bacceptance/firmware.elf') --timeout 90000 --scenario (Join-Path $PSScriptRoot 'scenario.yaml') --serial-log-file (Join-Path $evidence 'serial.txt') *> (Join-Path $evidence 'acceptance-cli.txt');if($LASTEXITCODE-ne 0){throw 'Stage 14B Wokwi simulation failed'}}finally{$env:WOKWI_CLI_TOKEN=$old;if($secret){$secret.Dispose()}}
python (Join-Path $PSScriptRoot 'evaluator_integrity.py')
python (Join-Path $PSScriptRoot 'evaluate.py')
python (Join-Path $PSScriptRoot 'structural_review.py')
