$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$firmware=Join-Path $repo 'firmware'
$evidence=Join-Path $PSScriptRoot 'evidence'
$python=Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
$cli=Join-Path $env:USERPROFILE '.wokwi\bin\wokwi-cli.exe'
New-Item -ItemType Directory -Force $evidence | Out-Null

& git -c "safe.directory=$repo" diff --quiet 09c03b45f8ecd9dce9f903f169cf52f0bf931ccb -- docs/ACCEPTANCE_TESTS_STAGE_9.md
if($LASTEXITCODE -ne 0){throw 'Frozen Stage 9 acceptance specification differs.'}

$files=@(
  'docs\ACCEPTANCE_TESTS_STAGE_9.md','firmware\build.ps1','firmware\platformio.ini',
  'firmware\src\main.cpp','firmware\include\pp\core.h','firmware\include\pp\session_definition.h',
  'firmware\include\pp\race_control.h','firmware\include\pp\race_engine.h',
  'firmware\include\pp\noticeboard.h','firmware\include\pp\browser_interface.h',
  'firmware\tests\stage9_acceptance_probe.inc','acceptance\stage9\scenario.yaml',
  'acceptance\stage9\run.ps1','acceptance\stage9\evaluate.py','acceptance\stage9\structural_review.py'
)
$manifest=@{};foreach($file in $files){$manifest[$file]=(Get-FileHash (Join-Path $repo $file) -Algorithm SHA256).Hash}
$manifest|ConvertTo-Json|Set-Content (Join-Path $evidence 'source-manifest.json')

& (Join-Path $firmware 'build.ps1') -Environment esp32dev *> (Join-Path $evidence 'esp32dev-build.txt');if($LASTEXITCODE -ne 0){throw 'Normal production build failed.'}
& (Join-Path $firmware 'build.ps1') -Environment stage9acceptance *> (Join-Path $evidence 'stage9acceptance-build.txt');if($LASTEXITCODE -ne 0){throw 'Stage 9 acceptance build failed.'}
& (Join-Path $firmware 'build.ps1') -Environment stage9demo *> (Join-Path $evidence 'stage9demo-build.txt');if($LASTEXITCODE -ne 0){throw 'Stage 9 demo build failed.'}

$identity=@{project='ConnalM/Plonk-and-Play';source_base_commit=(& git -c "safe.directory=$repo" rev-parse HEAD).Trim();frozen_acceptance_commit='09c03b45f8ecd9dce9f903f169cf52f0bf931ccb';source_manifest_sha256=(Get-FileHash (Join-Path $evidence 'source-manifest.json') -Algorithm SHA256).Hash;stage9_elf_sha256=(Get-FileHash (Join-Path $firmware '.pio\build\stage9acceptance\firmware.elf') -Algorithm SHA256).Hash;stage9demo_elf_sha256=(Get-FileHash (Join-Path $firmware '.pio\build\stage9demo\firmware.elf') -Algorithm SHA256).Hash}|ConvertTo-Json
Set-Content (Join-Path $evidence 'identity.json') $identity

$old=$env:WOKWI_CLI_TOKEN;$secret=$null
try {
  if(-not $old){$secret=Import-Clixml (Join-Path $env:USERPROFILE '.wokwi\cli-token.clixml');$env:WOKWI_CLI_TOKEN=[Net.NetworkCredential]::new('', $secret).Password}
  & $cli $firmware --elf (Join-Path $firmware '.pio\build\stage9acceptance\firmware.elf') --timeout 90000 --scenario (Join-Path $PSScriptRoot 'scenario.yaml') --serial-log-file (Join-Path $evidence 'serial-current.txt') *> (Join-Path $evidence 'acceptance-cli-current.txt')
  if($LASTEXITCODE -ne 0){throw 'Stage 9 Wokwi simulation failed.'}
} finally {$env:WOKWI_CLI_TOKEN=$old;if($secret){$secret.Dispose()}}

& $python (Join-Path $PSScriptRoot 'structural_review.py');if($LASTEXITCODE -ne 0){throw 'Stage 9 structural review failed.'}
& $python (Join-Path $PSScriptRoot 'evaluate.py');if($LASTEXITCODE -ne 0){throw 'Stage 9 evaluator failed.'}
