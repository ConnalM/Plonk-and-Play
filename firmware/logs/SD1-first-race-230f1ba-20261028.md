# SD-1 first-race checkpoint — 230f1ba

Date: 2026-10-28  
Firmware: `230f1ba09465f3751aa9916624b78848f2457925`  
Environment: `stage8bp1s3sd`  
Capture: `device-monitor-sd1-230f1ba-20261028.log`  
Capture SHA-256: `DE3C7C3D296EDDBEB947191D9A9EE348B4ED1534BA6DB8A4F899047E24AA1335`

## Result

**PASS — SD-1 first-race checkpoint.** One normal two-lane, two-lap race completed and produced exactly one new History commit, sequence 10.

## Decisive Serial evidence

- SD remained healthy throughout: repeated `P&P STAGE 8B-P1 REAL SD BOOT_SELF_TEST=PASS` markers, including after the race.
- Race accepted and progressed through both competitors:
  - `LAP accepted entry=1 lap=1`
  - `LAP accepted entry=2 lap=1`
  - `LAP accepted entry=1 lap=2`
  - `RESULT finish_settled relevant_us=2091245861`
- Final result sealed successfully:
  - `RESULT sealed finish_relevant_us=2091245861 winner_entry=1 valid=1 integrity=OK`
- Exactly one new durable History result was published:
  - `RESULT history_stored sequence=10 relevant_us=2091245861`
- No `persistence_fault`, `history_storage_fault`, panic, Guru Meditation, stack-canary or reset marker occurred after the race.
- Both Browser SSE generations continued notification/keepalive reporting; no send failure was observed.
- HTTP health/state/proposal/context traffic remained successful after settlement, with client population returning through the normal 9–11 range.

The pre-race baseline was sequence 9 and remained the prior committed record. The new sequence-10 result was sealed before `history_stored`, so the committed History identity is consistent with the completed Results/Details result observed in the Browser.

No reset, SD removal, remount, firmware change, or persistence repair was performed.
