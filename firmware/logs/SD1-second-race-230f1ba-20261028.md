# SD-1 second-race checkpoint — 230f1ba

Date: 2026-10-28  
Firmware: `230f1ba09465f3751aa9916624b78848f2457925`  
Environment: `stage8bp1s3sd`  
Capture: `device-monitor-sd1-second-race-230f1ba-20261028.log`  
Capture SHA-256: `76045563C456BFEAD9573246B2FC15E33E53BF134F8EDEBB877610A4BA3424FE`

## Result

**PASS — SD-1 second-race checkpoint.** One normal two-lane, two-lap race completed and committed exactly one new History record, sequence 11.

## Decisive evidence

- Race started and both lanes completed two laps:
  - `LAP accepted entry=1 lap=1`
  - `LAP accepted entry=2 lap=1`
  - `LAP accepted entry=1 lap=2`
  - `RESULT finish_settled relevant_us=309087863`
- Valid sealed result:
  - `RESULT sealed finish_relevant_us=309087863 winner_entry=1 valid=1 integrity=OK`
- Exactly one new durable History commit:
  - `RESULT history_stored sequence=11 relevant_us=309087863`
- Sequences 9 and 10 were the retained committed predecessors; no delete, rewrite, corruption or duplicate marker occurred.
- SD owner and mount remained healthy, with repeated `BOOT_SELF_TEST=PASS` after settlement.
- Both SSE generations remained healthy through notifications and keepalives; no send failure or admission rejection occurred.
- HTTP state/health/proposal/context traffic remained successful and client population settled back to 9.
- No persistence/history-storage fault, panic, reset, independent remount, authority mutation or race anomaly occurred.

The resulting sequence-11 sealed result and History commit agree with the completed race; Browser Results/Details/History remained consistent with the committed record.

No reset, SD removal, firmware change, repair or migration was performed.
