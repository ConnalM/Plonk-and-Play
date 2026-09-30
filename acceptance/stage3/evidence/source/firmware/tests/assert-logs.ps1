$ErrorActionPreference='Stop'
$logs=Join-Path $PSScriptRoot '..\.pio'
$normal=Get-Content (Join-Path $logs 'esp32dev-serial.log') -Raw
$quiet=Get-Content (Join-Path $logs 'quiet-serial.log') -Raw
$verification=Get-Content (Join-Path $logs 'verification-serial.log') -Raw
if (($normal+$quiet+$verification) -cmatch '\[TEST\] FAIL|STAGE1_FAIL|\[BUS SELF-TEST\] FAIL') { throw 'Failure in simulator logs.' }
if ([regex]::Matches($normal,'\[BUS SELF-TEST\] PASS').Count -ne 3) { throw 'Quiet/resume or self-test count incorrect.' }
if ($quiet -match '\[INIT\]|STAGE1_PASS|\[DEV\] P&P STAGE') { throw 'Quiet boot emitted startup diagnostics.' }
if ($quiet -notmatch 'Stage1 IDLE' -or $quiet -notmatch '\[BUS SELF-TEST\] PASS') { throw 'Quiet boot did not reach IDLE or pass self-test.' }
$times=[regex]::Matches($normal,'system_us=(\d+)')
if ($times.Count -lt 2 -or [uint64]$times[1].Groups[1].Value -le [uint64]$times[0].Groups[1].Value) { throw 'System Time did not advance across quiet/resume.' }
if ($verification -notmatch 'SW_CPU_RESET' -or $verification -notmatch 'PASS nvs.configuration.survives.reset' -or $verification -notmatch 'STAGE1_PASS') { throw 'Persistent configuration reset verification incomplete.' }
Write-Host 'PASS: serial suppression, repeatability, advancing System Time and reset persistence evidence.'
