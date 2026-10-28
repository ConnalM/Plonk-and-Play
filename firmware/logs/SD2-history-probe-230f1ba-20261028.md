# SD-2 missing-medium `/history` probe — 230f1ba

Date: 2026-10-28  
Firmware: `230f1ba09465f3751aa9916624b78848f2457925`  
Environment: `stage8bp1s3sd`  
Capture: `device-monitor-sd2-history-probe-230f1ba-20261028.log`  
Capture SHA-256: `C32CDF5DE38E9EE1163255BBF92A67DB6E8151F164114B604D3D48A873EFD2FE`

## Result

**INCONCLUSIVE — the single `/history` request did not reach the controller.** The SD card remained out and no second request was issued.

## Pre-probe state

- SD owner/health state remained at the prior healthy baseline.
- `/health` completed successfully.
- Both SSE sessions continued keepalives.
- No race or persistence operation was active.

## Probe outcome

One ordinary request was attempted against the known STA address `192.168.68.102/history`. The host returned an immediate connection failure (`curl: (7) Failed to connect`), and the Serial capture contains no `/history` handler entry, SD read failure, owner transition, remount, recovery, retry storm, panic or reset.

This probe therefore establishes no firmware behavior for a missing-medium History read. The failure boundary is the host/network path before the controller. The SD card remains out; no recovery, reinsertion, write, race or reset was performed.
