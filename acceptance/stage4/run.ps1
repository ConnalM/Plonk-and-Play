$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$firmware=Join-Path $repo 'firmware';$evidence=Join-Path $PSScriptRoot 'evidence'
$python=Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe';$cli=Join-Path $env:USERPROFILE '.wokwi\bin\wokwi-cli.exe'
& git -c "safe.directory=C:/Users/conna/Documents/Plonk-and-Play" diff --quiet eb6a1b8a4e7912c1d3b471ede96578c978f0a909 -- docs/ACCEPTANCE_TESTS_STAGE_4.md
if($LASTEXITCODE -ne 0){throw 'Frozen Stage 4 acceptance specification differs.'}
& (Join-Path $firmware 'build.ps1') -Environment esp32dev *> (Join-Path $evidence 'esp32dev-build.txt')
& (Join-Path $firmware 'build.ps1') -Environment stage4acceptance *> (Join-Path $evidence 'stage4acceptance-build.txt')
if($LASTEXITCODE -ne 0){throw 'Build failed.'}
$old=$env:WOKWI_CLI_TOKEN;$secret=$null
try {if(-not $old){$secret=Import-Clixml (Join-Path $env:USERPROFILE '.wokwi\cli-token.clixml');$env:WOKWI_CLI_TOKEN=[Net.NetworkCredential]::new('', $secret).Password}
& $cli $firmware --elf (Join-Path $firmware '.pio\build\stage4acceptance\firmware.elf') --timeout 60000 --scenario (Join-Path $PSScriptRoot 'scenario.yaml') --serial-log-file (Join-Path $evidence 'acceptance-serial.txt') *> (Join-Path $evidence 'acceptance-cli.txt')
if($LASTEXITCODE -ne 0){throw 'Wokwi acceptance failed.'}
} finally {$env:WOKWI_CLI_TOKEN=$old;if($secret){$secret.Dispose()}}
& $python (Join-Path $PSScriptRoot 'evaluate.py');if($LASTEXITCODE -ne 0){throw 'Acceptance checks failed.'}
