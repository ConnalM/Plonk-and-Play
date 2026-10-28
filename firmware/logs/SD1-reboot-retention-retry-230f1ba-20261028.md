# SD-1 reboot-retention retry — 230f1ba

Date: 2026-10-28  
Firmware: `230f1ba09465f3751aa9916624b78848f2457925`  
Environment: `stage8bp1s3sd`  
Capture: `device-monitor-sd1-retention-retry-230f1ba-20261028.log`  
Capture SHA-256: `69F47E3EFA42A81FEBCA33EF1863642E8FC73B28C67FE0D2656F341589CE8056`

## Result

**FAIL — Browser authority role restoration changed across RESET/EN.** This retry ruled out tablet sleep as the explanation for the previous missing-SSE observation: both Browser pages reconnected and completed fresh acquisition, but the Master/spectator roles were not preserved.

## Pre-reset baseline

- Two SSE sessions were healthy and settled (`generation=2` and `generation=3`).
- Tablet was `client_tag=2`, Master/Race Director (`is_master=true`).
- The laptop was the spectator.
- Client population was stable at approximately 8.

## Post-reset evidence

- Genuine ESP-ROM reboot marker captured.
- Provenance remained `source_commit=230f1ba09465f3751aa9916624b78848f2457925`, `environment=stage8bp1s3sd`.
- SD remounted successfully:
  - `SD owner mount OK`
  - `History store begin owner_lease=1 owner_mounted=1 owner_health=1`
  - `SD mount OK`
  - repeated `BOOT_SELF_TEST=PASS`
- Both Browser SSE sessions were admitted and settled:
  - generation 1 on fd 51;
  - generation 2 on fd 55;
  - no admission rejection, send failure, panic or reset loop.
- Both Browsers completed fresh `/state`, `/proposal`, `/context` acquisition.
- Fresh context results were:
  - `client_tag=1 ... is_master=true owner_connected=true`
  - `client_tag=2 ... is_master=false owner_connected=true`

## First failure boundary

The post-reset context made `client_tag=1` Master and `client_tag=2` spectator, reversing the pre-reset tablet-Master/laptop-spectator arrangement. Transport recovery and SD retention succeeded, but authority role restoration did not preserve the existing owner.

No race or persistence mutation was performed after reboot; no History record changed.
