# SD-1 reboot-retention retry — 230f1ba

Date: 2026-10-28  
Firmware: `230f1ba09465f3751aa9916624b78848f2457925`  
Environment: `stage8bp1s3sd`  
Capture: `device-monitor-sd1-retention-retry-230f1ba-20261028.log`  
Capture SHA-256: `69F47E3EFA42A81FEBCA33EF1863642E8FC73B28C67FE0D2656F341589CE8056`

## Result

**Transport recovery PASS; physical authority mapping inconclusive from Serial alone.** This retry ruled out tablet sleep as the explanation for the previous missing-SSE observation: both Browser pages reconnected and completed fresh acquisition. The capture shows one Master and one spectator after recovery, but `client_tag` is not a stable physical-device identity across reboot, so Serial alone cannot establish which page owns either tag.

## Pre-reset baseline

- Two SSE sessions were healthy and settled (`generation=2` and `generation=3`).
- The pre-reset physical roles were recorded by the operator; the Serial capture records only the assigned client tag.
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

## Evidence limitation

The earlier report incorrectly treated client tags as stable laptop/tablet identities and therefore called this a role reversal. That inference is withdrawn. The retry proves automatic transport recovery, fresh acquisition and a settled Master/spectator state, but the capture alone does not prove physical-device role preservation. No Browser UI evidence was captured that maps the post-reset tags to the two devices.

No race or persistence mutation was performed after reboot; no History record changed.
