$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$firmware=Join-Path $repo 'firmware'
$evidence=Join-Path $PSScriptRoot 'evidence'
$python=Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
$cli=Join-Path $env:USERPROFILE '.wokwi\bin\wokwi-cli.exe'
New-Item -ItemType Directory -Force $evidence | Out-Null
& git -c "safe.directory=C:/Users/conna/Documents/Plonk-and-Play" diff --quiet a715c2653a08b6112565f8f1b83d1218b059827b -- docs/ACCEPTANCE_TESTS_STAGE_3.md
if ($LASTEXITCODE -ne 0) { throw 'Frozen Stage 3 acceptance specification differs.' }
& $python (Join-Path $PSScriptRoot 'evaluate.py') prepare
if ($LASTEXITCODE -ne 0) { throw 'Evidence preparation failed.' }
foreach ($environment in @('esp32dev','stage3acceptance')) {
  & (Join-Path $firmware 'build.ps1') -Environment $environment *> (Join-Path $evidence "$environment-build.txt")
  if ($LASTEXITCODE -ne 0) { throw "Build failed: $environment" }
}
$previous=[Environment]::GetEnvironmentVariable('WOKWI_CLI_TOKEN','Process');$secret=$null
try {
  if (-not $previous) {$secret=Import-Clixml -LiteralPath (Join-Path $env:USERPROFILE '.wokwi\cli-token.clixml');$env:WOKWI_CLI_TOKEN=[Net.NetworkCredential]::new('', $secret).Password}
  & $cli $firmware --elf (Join-Path $firmware '.pio\build\esp32dev\firmware.elf') --timeout 30000 --expect-text STAGE3_PASS --fail-text STAGE3_FAIL --serial-log-file (Join-Path $evidence 'production-serial.txt') *> (Join-Path $evidence 'production-cli.txt')
  if ($LASTEXITCODE -ne 0) { throw 'Production boot failed.' }
  & $cli $firmware --elf (Join-Path $firmware '.pio\build\stage3acceptance\firmware.elf') --timeout 60000 --scenario (Join-Path $PSScriptRoot 'scenario.yaml') --fail-text STAGE3_FAIL --serial-log-file (Join-Path $evidence 'acceptance-serial.txt') *> (Join-Path $evidence 'acceptance-cli.txt')
  if ($LASTEXITCODE -ne 0) { throw 'Acceptance simulation failed.' }
} finally {[Environment]::SetEnvironmentVariable('WOKWI_CLI_TOKEN',$previous,'Process');if($secret){$secret.Dispose()}}
& $python (Join-Path $PSScriptRoot 'evaluate.py') evaluate
if ($LASTEXITCODE -ne 0) { throw 'Acceptance tests failed; inspect results.json.' }
