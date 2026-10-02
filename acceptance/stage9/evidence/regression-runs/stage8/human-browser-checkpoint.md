# Stage 8 human Browser checkpoint

**Status:** PASS  
**Observed:** 2026-10-01  
**Frozen Stage 8 specification:** `20469948784a64662ef96cb20637cfd6a59cc27b`  
**Automated source manifest:** `158E352DF5D97694C4BDD567CCD5BDD1D80198EFE589693A83C890207F5A9129`

## Environment

- Wokwi ESP32 simulation connected through the real Private IoT Gateway.
- Diagnostic Browser URL: `http://127.0.0.1:9080/`.
- The gateway health endpoint confirmed the Stage 8 Browser service: `{"ok":true,"browser":"stage8"}`.

## Observed checkpoint

1. The Browser connected and displayed `synchronised` with authoritative current State.
2. Selecting **START** produced a correlated `REQUEST_RESULT` of `ACCEPTED`.
3. The accepted request produced authoritative `RACING` State with `raceEntryId: 1`.
4. A later START while the lifecycle was already RACING produced its own correlated `REQUEST_RESULT` of `REJECTED`, with the reason `START is not valid in the current lifecycle state`.
5. That rejection left the authoritative RACING State and `raceEntryId: 1` unchanged. The Browser remained synchronised.
6. Ctrl+F5 created a fresh Browser instance which reconstructed the authoritative RACING State through the Noticeboard path, without a further race event.
7. The fresh Browser showed no Request Result until a new request was made. It correctly showed `Fact NONE`, revision `0`, because no lap event had occurred.

The brief `unsynchronised` display while refreshing State was observed to converge back to `synchronised`; it did not represent loss of authoritative State.

## Result

The required Stage 8 human Browser checkpoint has passed. This record is supplementary to the retained automated campaign evidence and does not replace it.