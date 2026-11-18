# Physical Taster XIP application build reference

The established Windows build uses the existing `bwp7welcome` Ninja tree and the `firmware/wp7_idf_arduino_dbuf` ESP-IDF project. The physical Taster target uses ESP32-S3, ESP-IDF 5.5.5 with Arduino as a component, Octal PSRAM at 80 MHz, XIP, and one full RGB framebuffer plus 20-line bounce buffers.

The physically accepted application image has SHA-256 `3649CEE0EE12EF4630EFD5A0825B57EB37B7C3603543E7A9093AA540691D1A08`. Its archived copy is `C:\Users\conna\Documents\P&P Backups\2026-11-18-WP8-centred-lamps\plonk-and-play-3649CEE0.bin`. Flash verification matched at application offset `0x10000`. On this document’s preparation, the existing build image and archived image both matched that SHA-256.

The instructions below are for a future authorised build. The build tree, firmware and recovery checkpoint were left intact during documentation.

## 1. Check the existing inputs

- The working tree is `C:\Users\conna\.codex\worktrees\framework-migration\Plonk-and-Play`. The CMake cache records this absolute source path. Keep the existing `bwp7welcome` directory in place.
- The ESP-IDF checkout is available at `Y:\`. The cache’s `SDKCONFIG` points to `X:\firmware\wp7_idf_arduino_dbuf\sdkconfig.xip`. Confirm both paths resolve, and make sure the file on `X:\` matches the intended project configuration.
- GFX and TAMC_GT911 inputs are at the established paths on `P:\` used below. The project’s `components/arduino` junction must resolve to the installed Arduino-ESP32 component. Verify the library paths before any invocation that might regenerate the build graph.
- The generated build uses Ninja 1.12.1, Xtensa toolchain `esp-14.2.0_20260121`, and the ESP-IDF Python 3.11 environment. Preserve `CMAKE_NINJA_FORCE_RESPONSE_FILE=ON` and the existing response files under `bwp7welcome/toolchain/`.
- Use the approved external execution permission under the build directory’s owner account. The sandbox group had read/execute access where Ninja needs to write and compact `.ninja_log` and `.ninja_deps`. Preserve those files and their permissions.

## 2. Run the established Ninja build

Set these in the same PowerShell process that invokes Ninja. The per-process environment lets CMake regenerate the existing graph with the same settings when inputs change.

```powershell
$ppRepo = 'C:\Users\conna\.codex\worktrees\framework-migration\Plonk-and-Play'
$ppBuild = Join-Path $ppRepo 'bwp7welcome'
$env:IDF_PATH = 'Y:\'
$env:IDF_TOOLS_PATH = 'C:\Users\conna\.espressif-wp7'
$env:IDF_PYTHON_ENV_PATH = Join-Path $env:IDF_TOOLS_PATH 'python_env\idf5.5_py3.11_env'
$env:PYTHON = Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe'
$ppNinjaDir = Join-Path $env:IDF_TOOLS_PATH 'tools\ninja\1.12.1'
$ppXtensaBin = Join-Path $env:IDF_TOOLS_PATH 'tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin'
$env:PATH = "$ppXtensaBin;$ppNinjaDir;$env:PATH"
$env:PP_ARDUINO_COMPONENT_DIR = 'C:\Users\conna\.platformio\packages\framework-arduinoespressif32'
$env:PP_GFX_ROOT = 'P:\firmware\.pio\libdeps\stage8bp1s3sd\GFX Library for Arduino'
$env:PP_TAMC_ROOT = 'P:\firmware\lib\TAMC_GT911\src'
$env:PP_WP7_IDF_SINGLE_FB_XIP = '1'
$env:PP_WP7_RGB_DRIVER_SIZE_OPT = '1'

$ppNinja = Join-Path $ppNinjaDir 'ninja.exe'
& $ppNinja -C $ppBuild -j 1
if ($LASTEXITCODE -ne 0) { throw "Ninja exited $LASTEXITCODE" }
```

The proven command uses the default Ninja target, which can run bootloader and partition-size checks alongside the application build. The deliverable for an application-only deployment is solely `bwp7welcome/plonk-and-play.bin`. The generated graph also declares an `app` target. The command above records the invocation used for the accepted workflow.

If a pass refreshes the graph without producing the expected changed application, let it finish and run the same command again. Verify compilation and linking against the changed sources before identifying the result as a new candidate. A timestamp or a hash change alone does not establish which code was compiled. A permission or path failure calls for checking the existing execution context, mappings, and generated flags before any recovery action.

## 3. Validate the application and retain evidence

1. Run `git diff --check` from the source tree. Check the intended changes against the build log, object and link outputs.
2. Inspect `bwp7welcome/compile_commands.json` for `PP_WP7_SINGLE_FB_RECOVERY=1`, `PP_WP7_DISABLE_ANIMATIONS=1`, and a translation-unit-specific `-Os` on `esp_lcd_panel_rgb.c`. The accepted warning sequence is enabled, so `PP_WP7_DISABLE_PAUSED_WARNING_LIGHTS=1` must be absent. Check `sdkconfig.xip` and its generated configuration for Octal PSRAM, XIP, 80 MHz PSRAM and `CONFIG_LCD_RGB_RESTART_IN_VSYNC=y`. The generated `PP_BUILD_ENVIRONMENT` string still says `stage8bp1s3sd-wp7-idf-double-buffer`; the actual compile defines and display source determine the framebuffer configuration.
3. Validate the image and record its SHA-256.

```powershell
$ppBin = Join-Path $ppBuild 'plonk-and-play.bin'
$ppEsptool = Join-Path $env:IDF_PATH 'components\esptool_py\esptool\esptool.py'
& $env:PYTHON $ppEsptool --chip esp32s3 image_info $ppBin
if ($LASTEXITCODE -ne 0) { throw "esptool image_info exited $LASTEXITCODE" }
Get-FileHash -Algorithm SHA256 -LiteralPath $ppBin
```

Confirm that the reported image checksum and validation hash are both valid. Record the application path, SHA-256, source revision and working-tree changes, relevant build settings, and the acceptance evidence for that exact image. Retain the archived milestone separately from the incremental build output. A future flash requires separate approval for the exact candidate hash and uses only the application offset.

## Reproduction boundary

This reference records the successful existing-tree build. A fresh-machine reproduction still needs the reviewed source inputs, artwork, SDK configuration, dependency lock, GT911 changes and externally supplied ESP-IDF, Arduino and GFX versions pinned in the proposed checkpoint. Keep the generated Ninja tree, build caches, executables, compiled firmware images and credentials out of the source checkpoint. Include the RGB565 welcome raster and lamp/logo/bezel artwork as build inputs.

# Controlled Git staging

Stage this application project by explicit source/configuration file paths only.
Never stage `firmware/wp7_idf_arduino_dbuf/` as a directory: it contains nested
`build*` output, generated toolchain data and caches. Before committing, run
`git diff --cached --check` and inspect `git diff --cached --name-only`; reject
all build directories, `.pio`, binaries, logs, caches and temporary probes.
