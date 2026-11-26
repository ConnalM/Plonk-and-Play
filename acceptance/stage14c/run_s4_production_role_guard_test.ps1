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

# Integrated diagnostics remain legal because no production role is selected.
Invoke-GuardCompile @('-DPP_PHASE8B_P1_TEST=1', '-DPP_S4_GUARD_EXPECT_INTEGRATED=1') $true
Invoke-GuardCompile @('-DPP_S4_PRODUCTION_CORE=1', '-DPP_S4_GUARD_EXPECT_CORE=1') $true
Invoke-GuardCompile @('-DPP_S4_PRODUCTION_TASTER=1', '-DPP_S4_GUARD_EXPECT_TASTER=1') $true

foreach ($flag in @('PP_PHASE8B_P1_TEST', 'PP_WP7_ACCEPTANCE_SIMULATION',
                    'PP_SIMULATED_RACING_TEST', 'PP_UPDATE_TEST_KEY',
                    'PP_WP7_PHYSICAL_RACING_TEST')) {
  Invoke-GuardCompile @('-DPP_S4_PRODUCTION_CORE=1', "-D$flag=1") $false "S4 production role rejects $flag"
}
Invoke-GuardCompile @('-DPP_S4_PRODUCTION_TASTER=1', '-DPP_STAGE14C_ACCEPTANCE=1') $false 'S4 production role rejects an acceptance, demo or diagnostic fixture flag'
Invoke-GuardCompile @('-DPP_S4_PRODUCTION_CORE=1', '-DPP_S4_PRODUCTION_TASTER=1') $false 'S4 production build must select exactly one role'

Write-Host 'S4 production-role guard contract PASS'
