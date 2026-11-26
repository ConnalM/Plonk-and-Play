$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

# This is a CMake script-mode contract only.  It neither configures ESP-IDF
# nor compiles/links a firmware application.
& cmake -P (Join-Path $PSScriptRoot 's4_role_profile_cmake_test.cmake')
if ($LASTEXITCODE -ne 0) {
  throw "S4b role-profile CMake contract failed: $LASTEXITCODE"
}
Write-Host 'S4b independent role build-profile contract PASS'
