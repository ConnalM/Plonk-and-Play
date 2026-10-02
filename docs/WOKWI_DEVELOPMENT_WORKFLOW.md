# Wokwi development workflow

## Why Restart previously loaded the wrong firmware

**Upload Firmware and Start Simulation** temporarily loads the file selected in
the Wokwi UI.  **Restart** then loads the firmware configured in
`firmware/wokwi.toml`.  That file previously named the normal `esp32dev` build,
so Restart discarded the manually selected demo image.

## Restartable development image

`firmware/wokwi.toml` now names the dedicated `wokwi-dev` build. It currently
uses the accepted Stage 10 demonstration configuration. It is separate from:

- `esp32dev`, the normal product build;
- `stage10demo`, the explicitly named Stage 10 demo build; and
- every acceptance environment.

From `firmware`, run:

```powershell
.\wokwi-dev.ps1
```

Then start or restart the simulation through **Wokwi CLI** or **Wokwi for VS
Code**, which reads the local configuration. No firmware file chooser is
needed. The expected Serial Monitor identity is:

```text
[DEV] P&P STAGE 10 DEMO -- diagnostics are not product State
[DEV] Stage10 Browser server=READY error=0
```

The normal build remains available through `./build.ps1 -Environment esp32dev`.
Acceptance work continues to select its own named environment and never relies
on `wokwi-dev`.

## Web custom-firmware project

The Wokwi web project's **Upload Firmware and Start Simulation** feature is a
temporary browser upload. Its **Restart** action restarts the cloud project and
cannot read this computer's `.pio` directory or local `wokwi.toml`; it will
therefore replace the temporary upload with the cloud project's configured
firmware. This is a Wokwi web limitation, not a P&P build failure.

For a Browser checkpoint using that web project:

1. Run `./wokwi-dev.ps1` after a code change.
2. Use **Upload Firmware and Start Simulation** once to load
   `.pio/build/wokwi-dev/firmware.merged.bin` into a newly created simulation.
3. For every later clean development reset of that same simulation, send
   `r` in the Serial Monitor. This performs an ESP32 soft reset and preserves
   the already-uploaded development image.

`r` exists only in the `wokwi-dev` environment. It is not present in normal,
stage demo, or acceptance firmware. Do not use Wokwi's web **Restart** while a
temporary development image is required.

## Local Browser route

When a Browser checkpoint needs the Wokwi gateway, keep the gateway running and
use its existing forward:

```powershell
& "C:\Users\conna\.wokwi\gateway-v2.0.1\wokwigw.exe" --forward 9080:10.13.37.2:80
```

The local diagnostic Browser address is `http://127.0.0.1:9080/`.  Building or
restarting the development image does not require any PowerShell action for an
already-running gateway.

## Updating the development target

When a later accepted stage needs a different local demonstration, intentionally
update only the `wokwi-dev` environment and this document's expected identity.
Do not redirect the normal or acceptance environments.
