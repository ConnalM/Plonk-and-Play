param([ValidateSet('esp32dev','verification','quiet')][string]$Environment='verification', [string]$Scenario='')
$ErrorActionPreference='Stop'
$cli=Join-Path $env:USERPROFILE '.wokwi\bin\wokwi-cli.exe'
$tokenFile=Join-Path $env:USERPROFILE '.wokwi\cli-token.clixml'
$oldToken=[Environment]::GetEnvironmentVariable('WOKWI_CLI_TOKEN','Process')
$secret=$null
try {
    if (-not $oldToken) {
        $secret=Import-Clixml -LiteralPath $tokenFile
        if ($secret -isnot [Security.SecureString]) { throw 'Invalid encrypted Wokwi credential.' }
        $env:WOKWI_CLI_TOKEN=[Net.NetworkCredential]::new('', $secret).Password
    }
    $elf=Join-Path $PSScriptRoot ".pio\build\$Environment\firmware.elf"
    $log=Join-Path $PSScriptRoot ".pio\$Environment-serial.log"
    $arguments=@($PSScriptRoot,'--elf',$elf,'--timeout','30000','--fail-text','FAIL','--serial-log-file',$log)
    if ($Scenario) { $arguments+=@('--scenario',(Join-Path $PSScriptRoot $Scenario)) }
    else { $arguments+=@('--expect-text','STAGE1_PASS') }
    & $cli @arguments
    if ($LASTEXITCODE -ne 0) { throw "Wokwi verification failed: $LASTEXITCODE" }
    Write-Host "PASS: $Environment (serial evidence: $log)"
} finally {
    [Environment]::SetEnvironmentVariable('WOKWI_CLI_TOKEN',$oldToken,'Process')
    if ($secret) { $secret.Dispose() }
}
