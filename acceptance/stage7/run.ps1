$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path;$firmware=Join-Path $repo 'firmware';$evidence=Join-Path $PSScriptRoot 'evidence'
$python=Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe';$cli=Join-Path $env:USERPROFILE '.wokwi\bin\wokwi-cli.exe'
New-Item -ItemType Directory -Force $evidence | Out-Null
& git -c "safe.directory=$repo" diff --quiet 4b6722a663bd1365c49a7dd279dcc721d3d14806 -- docs/ACCEPTANCE_TESTS_STAGE_7.md
if($LASTEXITCODE -ne 0){throw 'Frozen Stage 7 acceptance specification differs.'}
$files=@('firmware\build.ps1','firmware\platformio.ini','firmware\src\main.cpp','firmware\include\pp\core.h','firmware\include\pp\race_control.h','firmware\include\pp\race_engine.h','firmware\include\pp\session_definition.h','firmware\include\pp\noticeboard.h','firmware\include\pp\browser_interface.h','firmware\tests\stage7_acceptance_probe.inc','firmware\tests\stage7_demo_probe.inc','acceptance\stage7\scenario.yaml','acceptance\stage7\run.ps1','acceptance\stage7\evaluate.py')
$manifest=@{};foreach($file in $files){$manifest[$file]=(Get-FileHash (Join-Path $repo $file) -Algorithm SHA256).Hash};$manifest|ConvertTo-Json|Set-Content (Join-Path $evidence 'source-manifest.json')
& (Join-Path $firmware 'build.ps1') -Environment esp32dev *> (Join-Path $evidence 'esp32dev-build.txt');if($LASTEXITCODE -ne 0){throw 'Production build failed.'}
& (Join-Path $firmware 'build.ps1') -Environment stage7acceptance *> (Join-Path $evidence 'stage7acceptance-build.txt');if($LASTEXITCODE -ne 0){throw 'Acceptance build failed.'}
$identity=@{project='ConnalM/Plonk-and-Play';source_base_commit=(& git -c "safe.directory=$repo" rev-parse HEAD).Trim();frozen_acceptance_commit='4b6722a663bd1365c49a7dd279dcc721d3d14806';source_manifest_sha256=(Get-FileHash (Join-Path $evidence 'source-manifest.json') -Algorithm SHA256).Hash;stage7_elf_sha256=(Get-FileHash (Join-Path $firmware '.pio\build\stage7acceptance\firmware.elf') -Algorithm SHA256).Hash}|ConvertTo-Json
Set-Content (Join-Path $evidence 'identity.json') $identity
$old=$env:WOKWI_CLI_TOKEN;$secret=$null
try {if(-not $old){$secret=Import-Clixml (Join-Path $env:USERPROFILE '.wokwi\cli-token.clixml');$env:WOKWI_CLI_TOKEN=[Net.NetworkCredential]::new('', $secret).Password};& $cli $firmware --elf (Join-Path $firmware '.pio\build\stage7acceptance\firmware.elf') --timeout 60000 --scenario (Join-Path $PSScriptRoot 'scenario.yaml') --serial-log-file (Join-Path $evidence 'serial.txt') *> (Join-Path $evidence 'acceptance-cli.txt');if($LASTEXITCODE -ne 0){throw 'Stage 7 Wokwi simulation failed.'}} finally {$env:WOKWI_CLI_TOKEN=$old;if($secret){$secret.Dispose()}}
& $python (Join-Path $PSScriptRoot 'evaluate.py');if($LASTEXITCODE -ne 0){throw 'Stage 7 evaluator failed.'}
