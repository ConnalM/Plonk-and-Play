# SD-2 physical removal / degraded-state checkpoint — 230f1ba

Date: 2026-10-28  
Firmware: `230f1ba09465f3751aa9916624b78848f2457925`  
Environment: `stage8bp1s3sd`  
Capture: `device-monitor-sd2-baseline-230f1ba-20261028.log`  
Capture SHA-256: `731515034C846F5DF430639291CB11385DAEEF091B1F4A3570C62D566799DF02`

## Result

**FAIL / no detectable degraded transition in the observation window.** The SD card was physically removed while powered, but the running firmware did not emit an owner-unavailable/degraded transition during the bounded observation.

## Observed evidence

- Repeated `P&P STAGE 8B-P1 REAL SD BOOT_SELF_TEST=PASS` continued after removal.
- `/health` requests continued to complete successfully (approximately 25–27 ms).
- Both SSE sessions continued keepalives; no transport failure occurred.
- No `SD owner` recovery/remount sequence appeared after the initial healthy state.
- No persistence fault, fabricated write success, panic, reset, watchdog loop, authority mutation or race mutation occurred.
- No `/history` or diagnostic storage access was initiated during this checkpoint, so no consumer was forced to touch the removed medium.

## First boundary

The first boundary is **physical removal was not observed by the current idle/health workload**. The capture therefore proves absence of an autonomous remount and stable Browser/control behavior, but it does not demonstrate the required owner transition to unavailable/degraded or bounded read-failure behavior. The SD card was left out as instructed; no reinsertion or recovery was attempted.
