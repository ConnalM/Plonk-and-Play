$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$firmware=Join-Path $repo 'firmware';$evidence=Join-Path $PSScriptRoot 'evidence'
$python=Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe';$cli=Join-Path $env:USERPROFILE '.wokwi\bin\wokwi-cli.exe'
New-Item -ItemType Directory -Force $evidence | Out-Null
& git -c "safe.directory=$repo" diff --quiet cff214d6cb43d3e1367f703fc5ccd06be1d2642a -- docs/ACCEPTANCE_TESTS_STAGE_6.md
if($LASTEXITCODE -ne 0){throw 'Frozen Stage 6 acceptance specification differs.'}
$sourceFiles=@(
  'firmware\build.ps1','firmware\platformio.ini','firmware\src\main.cpp',
  'firmware\include\pp\core.h','firmware\include\pp\race_control.h',
  'firmware\include\pp\race_engine.h','firmware\include\pp\session_definition.h',
  'firmware\tests\stage6_acceptance_probe.inc','acceptance\stage6\scenario.yaml',
  'acceptance\stage6\run.ps1','acceptance\stage6\evaluate.py',
  'firmware\tests\stage6_demo_probe.inc','demonstrations\stage6\demo.yaml',
  'demonstrations\stage6\diagram.json','demonstrations\stage6\wokwi.toml'
)
$sourceManifest=@{}
foreach($relative in $sourceFiles){$sourceManifest[$relative]=(Get-FileHash (Join-Path $repo $relative) -Algorithm SHA256).Hash}
$sourceManifest|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $evidence 'source-manifest.json')
& (Join-Path $firmware 'build.ps1') -Environment esp32dev *> (Join-Path $evidence 'esp32dev-build.txt')
if($LASTEXITCODE -ne 0){throw 'Production build failed.'}
& (Join-Path $firmware 'build.ps1') -Environment stage6acceptance *> (Join-Path $evidence 'stage6acceptance-build.txt')
if($LASTEXITCODE -ne 0){throw 'Acceptance build failed.'}
$source=@{project='ConnalM/Plonk-and-Play';source_base_commit=(& git -c "safe.directory=$repo" rev-parse HEAD).Trim();source_manifest_sha256=(Get-FileHash (Join-Path $evidence 'source-manifest.json') -Algorithm SHA256).Hash;frozen_acceptance_commit='cff214d6cb43d3e1367f703fc5ccd06be1d2642a';stage6_elf_sha256=(Get-FileHash (Join-Path $firmware '.pio\build\stage6acceptance\firmware.elf') -Algorithm SHA256).Hash}|ConvertTo-Json
Set-Content -LiteralPath (Join-Path $evidence 'identity.json') -Value $source
$old=$env:WOKWI_CLI_TOKEN;$secret=$null
try {
  if(-not $old){$secret=Import-Clixml (Join-Path $env:USERPROFILE '.wokwi\cli-token.clixml');$env:WOKWI_CLI_TOKEN=[Net.NetworkCredential]::new('', $secret).Password}
  & $cli $firmware --elf (Join-Path $firmware '.pio\build\stage6acceptance\firmware.elf') --timeout 60000 --scenario (Join-Path $PSScriptRoot 'scenario.yaml') --serial-log-file (Join-Path $evidence 'acceptance-serial.txt') *> (Join-Path $evidence 'acceptance-cli.txt')
  if($LASTEXITCODE -ne 0){throw 'Wokwi acceptance simulation failed.'}
} finally {$env:WOKWI_CLI_TOKEN=$old;if($secret){$secret.Dispose()}}
& $python (Join-Path $PSScriptRoot 'evaluate.py')
if($LASTEXITCODE -ne 0){throw 'Acceptance checks failed.'}
