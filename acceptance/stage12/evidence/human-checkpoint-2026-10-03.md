# Stage 12 human checkpoint — PASS

Date: 2026-10-03

Environment: `wokwi-dev` Stage 12 demo firmware through the private Wokwi gateway at `127.0.0.1:9080`.

Observed evidence:

- Clean READY state was established with no Race Director, zero laps, valid results, race integrity OK, and Fact `NONE`.
- The Browser acquired Race Director authority deliberately; passive opening did not.
- START entered RACING with two zero-lap entries.
- Lane 1 and Lane 2 momentary detector controls produced one passage each; both reached one lap.
- A further Lane 1 passage produced FINISHED with Lane 1 at two laps and Lane 2 at one lap.
- Human-readable Results showed the sealed winning/finish times, lane standings, laps-behind information, and lap times in seconds.
- Details showed immutable per-lane lap records in seconds.
- BACK returned to the finished summary.
- A detector trigger after FINISHED was rejected with `Start the race before triggering a simulated car.` and authoritative State/result remained unchanged.
- Browser refresh reconstructed the same FINISHED State, sealed result, and latest Fact.
- Race Again returned READY while retaining the completed result and Race Director authority.
- NEXT RACE: 3 LAPS changed only the proposed next setup.
- START created a fresh RACING session with fresh entry IDs 3 and 4 and both lanes at zero laps.

Result: PASS.
