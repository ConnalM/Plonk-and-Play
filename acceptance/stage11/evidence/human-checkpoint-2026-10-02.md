# Stage 11 human Browser checkpoint ledger

Date: 2026-10-03

This ledger separates automated evidence from observations made in the real Browser/Wokwi checkpoint. It does not convert automated evidence into human evidence.

| Test | Result | Evidence class | Evidence |
|---|---|---|---|
| 11.1 | PASS | Wokwi production-path | `serial-momentary2.txt` (`11.1`) |
| 11.2 | PASS | Wokwi production-path | `serial-momentary2.txt` (`11.2`) |
| 11.3 | PASS | Wokwi production-path | `serial-momentary2.txt` (`11.3`) |
| 11.4 | PASS | Wokwi production-path | `serial-momentary2.txt` (`11.4`) |
| 11.5 | PASS | Wokwi production-path | `serial-momentary2.txt` (`11.5`) |
| 11.6 | PASS | Wokwi production-path | `serial-momentary2.txt` (`11.6`) |
| 11.7 | PASS | Wokwi production-path | `serial-momentary2.txt` (`11.7`) |
| 11.8 | PASS | Wokwi production-path | `serial-momentary2.txt` (`11.7` plus probe timing assertions) |
| 11.9 | PASS | Wokwi production-path | `serial-momentary2.txt` (`11.9`) |
| 11.10 | PASS | Wokwi production-path | `serial-momentary2.txt` (`11.10`) |
| 11.11 | PASS | Wokwi production-path | `serial-momentary2.txt` (`11.11`) |
| 11.12 | PASS | Wokwi production-path | `serial-momentary2.txt` (`11.12`) |
| 11.13 | PASS | real HTTP | `http-acceptance.txt` and `http_acceptance.py` |
| 11.14 | PASS | real HTTP/Wokwi production-path | `http-acceptance.txt`; reconstruction assertions in runner |
| 11.15 | PASS | Wokwi production-path | `serial-momentary2.txt` (`11.15`) |
| 11.16 | PASS | Wokwi production-path | `serial-momentary2.txt` (`11.16`) |
| 11.17 | PASS | Wokwi production-path | `serial-momentary2.txt` (`11.17`) |
| 11.18 | PASS | structural/static | `structural_review.py` output and `serial-momentary2.txt` |
| 11.19 | PASS | regression | `serial-momentary2.txt`; applicable Stage 1–10 review evidence |
| 11.20 | PASS | Wokwi production-path/timing | `serial-momentary2.txt` (`11.20`) |
| 11.T | PASS | deliberate evaluator failure | `serial-momentary2.txt` (`wrong=FAIL restored=PASS`) |

## Final human checkpoint result

The complete Stage 11 human checkpoint is PASS / ACCEPTED.

Final evidence:

- READY pre-start simulated detector rejection: operator message `Start the race before triggering a simulated car.`; State remained READY 0/0 and Fact remained NONE.
- Race Director acquisition and START succeeded.
- First PAUSE: `pauseEffectiveAt=90035315`; Latest Fact `PAUSED`, revision 1, relevantTime 90035315.
- Grid Restart: Fact progression `PAUSED` revision 1 -> `RESTART_SCHEDULED` revision 2 -> `RESUMED` revision 3; lifecycle returned to RACING with GRID method.
- Repeated PAUSE: `pauseEffectiveAt=134713980`; Latest Fact `PAUSED`, revision 4, matching relevantTime.
- Honour Restart: Fact progression `PAUSED` revision 4 -> `RESTART_SCHEDULED` revision 5 -> `RESUMED` revision 6; lifecycle returned to RACING with HONOUR method.
- Race integrity remained OK throughout.
- Previously recorded spectator reconstruction, Honour timing, Grid Restart, countdown, detector, reset, and reconnect observations remain retained as PASS.

Result: PASS / ACCEPTED.
