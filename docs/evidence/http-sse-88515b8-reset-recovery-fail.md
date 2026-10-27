# 88515b8 RESET/EN Browser Recovery Validation — FAIL

Date: 2026-10-27

Firmware was the approved preserved artifact for source commit `88515b88f4f972da52d3b6a89888483189ff1b3f`, environment `stage8bp1s3sd`, SHA-256 `597BD42EE40A90B77BE18449C9811A0BDED64039ACDB33C08E8C57B7794B9E23`.

Capture: `firmware/logs/device-monitor-88515b8-reset-recovery.log`.

## Result

The dedicated RESET/EN action produced a genuine ESP-ROM boot marker while COM7 capture remained available. Boot provenance, SD mount, AP/STA, mDNS, HTTP and display initialization all succeeded.

Both existing Browser pages initially reconnected and fresh acquisition traffic appeared. The laptop reconstructed as spectator (`client_tag=1`, `is_master=false`) and the tablet reconstructed as Master (`client_tag=2`, `is_master=true`).

The first complete recovery failure boundary was:

`generation=5 ... stage=send_failure`

After that failure, replacement `/events` attempts were rejected with `ordinary_capacity_exhausted` at client populations 12, 11 and 10 (`result=257`). One replacement eventually admitted (`generation=6`), and fresh state/proposal/context acquisition completed for `client_tag=1`, but no second settled healthy SSE or fresh tablet Master context was captured before the run was stopped.

The controller remained alive. No panic, reset loop, race mutation, persistence mutation or authority-transfer request was observed. This is a physical validation FAIL for automatic two-Browser reboot recovery, not a firmware provenance or boot failure.

