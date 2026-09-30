$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$firmware=Join-Path $repo 'firmware'
$evidence=Join-Path $PSScriptRoot 'evidence'
$python=Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
$cli=Join-Path $env:USERPROFILE '.wokwi\bin\wokwi-cli.exe'
New-Item -ItemType Directory -Force $evidence | Out-Null
# The frozen criteria must be exactly the agreed pre-implementation revision.
$frozen=& git -c "safe.directory=$repo" show 'bc6693061f2ed4ad99b51d7109c20f271c7aa1e0:docs/ACCEPTANCE_TESTS_STAGE_2.md'
$current=Get-Content (Join-Path $repo 'docs\ACCEPTANCE_TESTS_STAGE_2.md')
if (Compare-Object $frozen $current) { throw 'Frozen Stage 2 acceptance specification differs.' }
& $python (Join-Path $PSScriptRoot 'evaluate.py') prepare
if ($LASTEXITCODE -ne 0) { throw 'Evidence preparation failed.' }
foreach ($environment in @('esp32dev','stage2acceptance')) {
    & (Join-Path $firmware 'build.ps1') -Environment $environment *> (Join-Path $evidence "$environment-build.txt")
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $environment" }
}
$previous=[Environment]::GetEnvironmentVariable('WOKWI_CLI_TOKEN','Process')
$secret=$null
try {
    if (-not $previous) {
        $secret=Import-Clixml -LiteralPath (Join-Path $env:USERPROFILE '.wokwi\cli-token.clixml')
        $env:WOKWI_CLI_TOKEN=[Net.NetworkCredential]::new('', $secret).Password
    }
    & $cli $firmware --elf (Join-Path $firmware '.pio\build\esp32dev\firmware.elf') --timeout 30000 --expect-text STAGE2_PASS --fail-text STAGE2_FAIL --serial-log-file (Join-Path $evidence 'production-serial.txt') *> (Join-Path $evidence 'production-cli.txt')
    if ($LASTEXITCODE -ne 0) { throw 'Production boot failed; inspect retained CLI output.' }
    & $cli $firmware --elf (Join-Path $firmware '.pio\build\stage2acceptance\firmware.elf') --timeout 60000 --scenario (Join-Path $PSScriptRoot 'scenario.yaml') --fail-text STAGE2_FAIL --serial-log-file (Join-Path $evidence 'acceptance-serial.txt') *> (Join-Path $evidence 'acceptance-cli.txt')
    if ($LASTEXITCODE -ne 0) { throw 'Acceptance simulation failed; inspect retained CLI output.' }
} finally {
    [Environment]::SetEnvironmentVariable('WOKWI_CLI_TOKEN',$previous,'Process')
    if ($secret) { $secret.Dispose() }
}
& $python (Join-Path $PSScriptRoot 'evaluate.py') evaluate
if ($LASTEXITCODE -ne 0) { throw 'Acceptance tests failed; inspect results.json.' }
