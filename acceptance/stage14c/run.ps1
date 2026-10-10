$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent|Split-Path -Parent;$firmware=Join-Path $root 'firmware';$evidence=Join-Path $PSScriptRoot 'evidence';New-Item -ItemType Directory -Force $evidence|Out-Null
& (Join-Path $firmware 'build.ps1') -Environment stage14cacceptance
& (Join-Path $firmware 'build.ps1') -Environment stage14cdemo
python (Join-Path $PSScriptRoot 'loop_stack_guard.py') --elf (Join-Path $firmware '.pio/build/stage14cacceptance/firmware.elf') --elf (Join-Path $firmware '.pio/build/stage14cdemo/firmware.elf')
$cli=Join-Path $env:USERPROFILE '.wokwi/bin/wokwi-cli.exe';$token=Join-Path $env:USERPROFILE '.wokwi/cli-token.clixml';$old=$env:WOKWI_CLI_TOKEN;$secret=$null
try{if(-not $old){$secret=Import-Clixml $token;$env:WOKWI_CLI_TOKEN=[Net.NetworkCredential]::new('',$secret).Password};& $cli $firmware --elf (Join-Path $firmware '.pio/build/stage14cacceptance/firmware.elf') --timeout 90000 --scenario (Join-Path $PSScriptRoot 'scenario.yaml') --serial-log-file (Join-Path $evidence 'serial.txt') *> (Join-Path $evidence 'acceptance-cli.txt');if($LASTEXITCODE-ne 0){throw 'Stage 14C Wokwi simulation failed'}}finally{$env:WOKWI_CLI_TOKEN=$old;if($secret){$secret.Dispose()}}
python (Join-Path $PSScriptRoot 'evaluator_integrity.py');python (Join-Path $PSScriptRoot 'evaluate.py');python (Join-Path $PSScriptRoot 'structural_review.py')
node (Join-Path $PSScriptRoot 'browser_proposal_runtime.js') | Tee-Object -FilePath (Join-Path $evidence 'browser-proposal-runtime.txt')
node (Join-Path $PSScriptRoot 'browser_recovery_runtime.js') | Tee-Object -FilePath (Join-Path $evidence 'browser-recovery-runtime.txt')
node (Join-Path $PSScriptRoot 'browser_presentation_runtime.js') | Tee-Object -FilePath (Join-Path $evidence 'browser-presentation-runtime.txt')
node (Join-Path $PSScriptRoot 'browser_normal_poll_runtime.js') | Tee-Object -FilePath (Join-Path $evidence 'browser-normal-poll-runtime.txt')
node (Join-Path $PSScriptRoot 'browser_results_runtime.js') | Tee-Object -FilePath (Join-Path $evidence 'browser-results-runtime.txt')
python (Join-Path $PSScriptRoot 'browser_integration_review.py') --acceptance-bin (Join-Path $firmware '.pio/build/stage14cacceptance/firmware.merged.bin') --demo-bin (Join-Path $firmware '.pio/build/stage14cdemo/firmware.merged.bin') | Tee-Object -FilePath (Join-Path $evidence 'browser-integration-review.txt')
python (Join-Path $PSScriptRoot 'presentation_corrections.py') | Tee-Object -FilePath (Join-Path $evidence 'presentation-corrections.txt')
