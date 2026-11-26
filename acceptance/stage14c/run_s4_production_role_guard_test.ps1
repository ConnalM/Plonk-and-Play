$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$include = Join-Path $root 'firmware\include'
$source = Join-Path $PSScriptRoot 's4_production_role_guard_test.cpp'
$out = Join-Path $env:TEMP 'pp-s4-production-role-guard-test.exe'

function Invoke-GuardCompile([string[]]$Definitions, [bool]$ExpectSuccess,
                             [string]$ExpectedDiagnostic = '') {
  $output = & g++ -std=c++17 -Wall -Wextra -Werror -I $include @Definitions $source -o $out 2>&1
  if ($ExpectSuccess) {
    if ($LASTEXITCODE -ne 0) { throw "S4 production guard positive compile failed: $output" }
    return
  }
  if ($LASTEXITCODE -eq 0) { throw "S4 production guard accepted forbidden definitions: $Definitions" }
  $text = $output -join [Environment]::NewLine
  if ($text -notmatch [regex]::Escape($ExpectedDiagnostic)) {
    throw "S4 production guard reported the wrong diagnostic: $text"
  }
}

# This list is kept in parity with pp_s4_production_forbidden_definitions().
$forbidden = @(
  'PP_PHASE8B_P1_TEST', 'PP_WP7_ACCEPTANCE_SIMULATION',
  'PP_SIMULATED_RACING_TEST', 'PP_UPDATE_TEST_KEY',
  'PP_WP7_PHYSICAL_RACING_TEST', 'PP_WP6_ROLLBACK_ACCEPTANCE',
  'PP_ACCEPTANCE', 'PP_VERIFY', 'PP_WP8_NVS_DIAGNOSTIC',
  'PP_STAGE8A_TASTER_WOKWI', 'PP_WP7_TOUCH_TEST', 'PP_WP7_READY_TEST',
  'PP_STAGE15_PHASE8_SD', 'PP_STAGE2_ACCEPTANCE', 'PP_STAGE3_ACCEPTANCE',
  'PP_STAGE4_ACCEPTANCE', 'PP_STAGE5_ACCEPTANCE', 'PP_STAGE6_ACCEPTANCE',
  'PP_STAGE6_DEMO', 'PP_STAGE7_ACCEPTANCE', 'PP_STAGE7_DEMO',
  'PP_STAGE8_ACCEPTANCE', 'PP_STAGE9_ACCEPTANCE', 'PP_STAGE9_DEMO',
  'PP_STAGE10_ACCEPTANCE', 'PP_STAGE10_DEMO', 'PP_STAGE11_ACCEPTANCE',
  'PP_STAGE11_DEMO', 'PP_STAGE12_ACCEPTANCE', 'PP_STAGE12_DEMO',
  'PP_STAGE13_ACCEPTANCE', 'PP_STAGE13_DEMO', 'PP_STAGE14A_ACCEPTANCE',
  'PP_STAGE14A_DEMO', 'PP_STAGE14B_ACCEPTANCE', 'PP_STAGE14B_DEMO',
  'PP_STAGE14C_ACCEPTANCE', 'PP_STAGE14C_DEMO'
)
$mandatory = @('PP_PHASE8B_P1_TEST', 'PP_WP7_ACCEPTANCE_SIMULATION',
               'PP_SIMULATED_RACING_TEST', 'PP_UPDATE_TEST_KEY',
               'PP_WP7_PHYSICAL_RACING_TEST')

# Integrated diagnostics remain legal because no production role is selected.
Invoke-GuardCompile @('-DPP_UPDATE_TEST_KEY=0', '-DPP_S4_GUARD_EXPECT_INTEGRATED=1') $true
foreach ($role in @('CORE', 'TASTER')) {
  Invoke-GuardCompile @("-DPP_S4_PRODUCTION_$role=1", "-DPP_S4_GUARD_EXPECT_$role=1") $true
  foreach ($flag in $forbidden) {
    $diagnostic = if ($mandatory -contains $flag) {
      "S4 production role rejects $flag"
    } else {
      'S4 production role rejects an acceptance, demo or diagnostic fixture flag'
    }
    Invoke-GuardCompile @("-DPP_S4_PRODUCTION_$role=1", "-D$flag=1") $false $diagnostic
  }
  foreach ($assigned in @('PP_UPDATE_TEST_KEY=1', 'PP_WP7_PHYSICAL_RACING_TEST=0', 'PP_STAGE14C_ACCEPTANCE=reviewed')) {
    $name = $assigned.Split('=')[0]
    $diagnostic = if ($mandatory -contains $name) {
      "S4 production role rejects $name"
    } else {
      'S4 production role rejects an acceptance, demo or diagnostic fixture flag'
    }
    Invoke-GuardCompile @("-DPP_S4_PRODUCTION_$role=1", "-D$assigned") $false $diagnostic
  }
}
Invoke-GuardCompile @('-DPP_S4_PRODUCTION_CORE=1', '-DPP_S4_PRODUCTION_TASTER=1') $false 'S4 production build must select exactly one role'

Write-Host 'S4 production-role guard contract PASS'
