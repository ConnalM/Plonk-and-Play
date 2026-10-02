# Stage 9 human Browser checkpoint — PASS

The checkpoint was observed through the isolated `stage9demo` firmware,
the Wokwi Private IoT Gateway, and the real diagnostic Browser at
`http://127.0.0.1:9080/`.

Observed sequence:

1. The Browser connected and synchronised to READY State.
2. A normal Browser START request was accepted and the authoritative State
   entered RACING with Race Entry 1 and Race Entry 2.
3. The Lane 1 source produced `LAP_COMPLETED` for Race Entry 1. Browser State
   showed Entry 1 on lap 1 while Entry 2 remained on lap 0.
4. A fresh Browser instance reconstructed that State through the Noticeboard
   path and returned to synchronised.
5. The independent Lane 2 source produced `LAP_COMPLETED` for Race Entry 2.
   Browser State showed both entries on lap 1, demonstrating no cross-lane
   contamination.
6. A second Lane 1 crossing produced Entry 1 lap 2. The race entered FINISHED;
   Entry 2 correctly remained on lap 1 under the Stage 9 immediate-finish
   fixture.

Final Browser State was synchronised, FINISHED, `resultValid: true`, and
`raceIntegrity: "OK"`. This satisfies the frozen Stage 9 human Browser
checkpoint.
