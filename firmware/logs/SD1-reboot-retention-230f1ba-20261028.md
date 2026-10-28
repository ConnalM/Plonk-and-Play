# SD-1 reboot-retention checkpoint — 230f1ba

Date: 2026-10-28  
Firmware: `230f1ba09465f3751aa9916624b78848f2457925`  
Environment: `stage8bp1s3sd`  
Capture: `device-monitor-sd1-retention-230f1ba-20261028.log`  
Capture SHA-256: `5610D746F1DEA2D1C727CBC4E15112F20570EB6D6A11145E50CC6E03780B5761`

## Result

**FAIL — automatic two-Browser recovery did not complete.** The RESET/EN reboot and SD retention path itself booted successfully, but only one of the two pre-existing Browser SSE sessions re-established within the bounded recovery observation.

## Proven evidence

- Genuine ESP-ROM reboot marker captured:
  - `ESP-ROM:esp32s3-20210327`
  - `rst:0x1 (POWERON),boot:0x8 (SPI_FAST_FLASH_BOOT)`
- Normal provenance returned:
  - `source_commit=230f1ba09465f3751aa9916624b78848f2457925`
  - `environment=stage8bp1s3sd`
- SD retention/remount succeeded:
  - `SD owner mount OK`
  - `History store begin owner_lease=1 owner_mounted=1 owner_health=1`
  - `SD mount OK`
  - repeated `BOOT_SELF_TEST=PASS`
- Normal AP/STA/mDNS/HTTP startup completed; `GOT_IP ip=192.168.68.102` was captured.
- One Browser automatically reconnected:
  - `/events fd=53` admitted with `sse_slots=1`
  - fresh `/state -> /proposal -> /context` completed
  - context reported `client_tag=1 ... is_master=true owner_connected=true`
- The second pre-reboot SSE generation never produced a post-reboot `/events` admission during the bounded observation. No second fresh SSE, second acquisition sequence, or final two-stream settlement was observed.
- No persistence fault, panic, reset loop, race mutation, or authority mutation was observed after the deliberate reset.

## Classification

The first complete failure boundary is **missing automatic recovery of the second Browser SSE session**. The SD reboot-retention portion is healthy, but the combined SD-1 reboot-retention acceptance criterion is not met.

No Browser was refreshed or otherwise touched after RESET/EN, and no further physical action was requested.
