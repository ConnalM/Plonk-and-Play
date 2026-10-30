# PSF.28 physical acceptance — 30 October 2026

## Candidate identity

- Source: `0b1f04509691f54d97a94cdce74f4d92b2466150`
- Artifact/evidence: `1edac88e0e3ff5f120117f2b268a76e213493fac`
- Environment: `stage8bp1s3sd-psf28`
- Firmware SHA-256: `2CFFC3985734AA995776B10546729AA1D3A012BA58FF769D3AF2B7D48E13A7BA`

## Result

**PSF.28: CLOSED / PASS.**

| Trial | Physical stimulus | Result |
|---|---|---|
| 1 | Real power interruption inside `PSF28_POWER_CUT_WINDOW` | PASS; reboot stable; interrupted result absent from official History |
| 2 | Real power interruption inside `PSF28_POWER_CUT_WINDOW` | PASS; reboot stable; interrupted result absent from official History |
| 3 | Real power interruption inside `PSF28_POWER_CUT_WINDOW` | PASS; reboot stable; interrupted result absent from official History |
| Control | Window expired normally; no power interruption | PASS; valid sealed sequence 14 committed exactly once; no duplicate |

Across all trials, previously committed records remained valid, temporary or
uncommitted data was not reconstructed as official History, and no physical SD
failure, panic, watchdog loop, recovery loop or authority mutation occurred.

## Evidence interpretation

The System Health `256` value is the Diagnostics backend's bounded
`retainedRecords`/capacity, not a History-record count. The earlier `/history`
`result=260` is `ESP_ERR_INVALID_SIZE`, not a 260-record namespace. Diagnostic
storage status values are `2 = Degraded` and `3 = Fault`. The observed `2 -> 3`
transition is confined to the separate bounded Diagnostics backend rollover /
append path; it is not a PSF.28 History or physical-SD failure. This remains a
separate non-WP5 follow-up observation and is intentionally not changed here.
