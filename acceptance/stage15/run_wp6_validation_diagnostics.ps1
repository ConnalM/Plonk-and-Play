$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $PSScriptRoot 'wp6_validation_diagnostics_test.exe'
$src = Join-Path $PSScriptRoot 'wp6_validation_diagnostics_test.cpp'
$include = Join-Path $root '..\firmware\include'
& g++ -std=c++17 -Wall -Wextra -Werror -I $include $src -o $out
if ($LASTEXITCODE -ne 0) { throw 'WP6 validation diagnostics contract compile failed' }
& $out
if ($LASTEXITCODE -ne 0) { throw 'WP6 validation diagnostics contract failed' }

$adapter = Get-Content (Join-Path $root '..\firmware\include\pp\esp32_firmware_update_adapter.h') -Raw
foreach ($marker in @(
  'FirmwareValidationDiagnosticStage::Ppu1Verifier',
  'FirmwareValidationDiagnosticStage::EspOtaEnd',
  'FirmwareValidationDiagnosticStage::PartitionDescription',
  'FirmwareValidationDiagnosticStage::ImageIdentity',
  'FirmwareValidationDiagnosticStage::StateMachine',
  'recordFirstFirmwareValidationDiagnostic',
  'view.validationDiagnostic = adapter_.validationDiagnostic();',
  'stateMachine_.abortRequest(adapterValidationFailure('
)) {
  if ($adapter.IndexOf($marker, [System.StringComparison]::Ordinal) -lt 0) { throw "Missing adapter diagnostic boundary: $marker" }
}

$browser = Get-Content (Join-Path $root '..\firmware\include\pp\browser_interface.h') -Raw
foreach ($marker in @('validationStage', 'validationReason', 'validationCode', 'firmwareUpdateDiagnosticStageText', 'firmwareUpdateDiagnosticReasonText')) {
  if ($browser.IndexOf($marker, [System.StringComparison]::Ordinal) -lt 0) { throw "Missing bounded status diagnostic field: $marker" }
}
Write-Output 'WP6_VALIDATION_DIAGNOSTICS_CONTRACT_PASS'
