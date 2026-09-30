param([ValidateSet('esp32dev','verification','quiet','acceptance','stage2acceptance','stage3acceptance','stage4acceptance','stage5acceptance')][string]$Environment = 'esp32dev')
$ErrorActionPreference = 'Stop'
$pioRoot = Join-Path $env:USERPROFILE '.platformio'
$python = Join-Path $pioRoot 'penv\Scripts\python.exe'
$packages = Join-Path $pioRoot 'packages'
$framework = Join-Path $packages 'framework-arduinoespressif32'
$esptool = Join-Path $packages 'tool-esptoolpy\esptool.py'
$output = Join-Path $PSScriptRoot ".pio\build\$Environment"

# Fail early if the previously inspected toolset is absent. Never install tools here.
$required = @(
    $python, $esptool,
    (Join-Path $pioRoot 'platforms\espressif32\platform.json'),
    (Join-Path $framework 'tools\partitions\boot_app0.bin'),
    (Join-Path $packages 'toolchain-xtensa-esp32\bin\xtensa-esp32-elf-g++.exe'),
    (Join-Path $packages 'tool-scons\scons.py')
)
foreach ($item in $required) {
    if (-not (Test-Path -LiteralPath $item)) { throw "Required installed tool is missing: $item" }
}
$platform = Get-Content (Join-Path $pioRoot 'platforms\espressif32\platform.json') -Raw | ConvertFrom-Json
if ($platform.version -ne '6.9.0') { throw 'This probe requires the existing Espressif32 6.9.0 platform.' }

# Temporary process environment only: avoid background update checks and telemetry.
$settings = @{
    PLATFORMIO_SETTING_ENABLE_TELEMETRY = 'false'
    # In Core 6.1.18, zero means check every run. Defer checks instead.
    PLATFORMIO_SETTING_CHECK_PLATFORMIO_INTERVAL = '1000000'
    PLATFORMIO_SETTING_CHECK_PRUNE_SYSTEM_THRESHOLD = '0'
    PYTHONDONTWRITEBYTECODE = '1'
}
$previous = @{}
foreach ($key in $settings.Keys) {
    $previous[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
    [Environment]::SetEnvironmentVariable($key, $settings[$key], 'Process')
}
try {
    & $python -B -m platformio run --project-dir $PSScriptRoot --environment $Environment
    if ($LASTEXITCODE -ne 0) { throw "PlatformIO build failed (exit $LASTEXITCODE)." }

    # Standard 4 MB ESP32 flash layout from the selected installed board/framework.
    & $python -B $esptool --chip esp32 merge_bin `
        -o (Join-Path $output 'firmware.merged.bin') `
        --flash_mode dio --flash_freq 40m --flash_size 4MB `
        0x1000 (Join-Path $output 'bootloader.bin') `
        0x8000 (Join-Path $output 'partitions.bin') `
        0xe000 (Join-Path $framework 'tools\partitions\boot_app0.bin') `
        0x10000 (Join-Path $output 'firmware.bin')
    if ($LASTEXITCODE -ne 0) { throw "Firmware merge failed (exit $LASTEXITCODE)." }
    Write-Host "Browser-loadable firmware: $(Join-Path $output 'firmware.merged.bin')"
} finally {
    foreach ($key in $settings.Keys) {
        [Environment]::SetEnvironmentVariable($key, $previous[$key], 'Process')
    }
}
