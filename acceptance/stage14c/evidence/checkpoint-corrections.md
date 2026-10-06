# Stage 14C checkpoint corrections

The real-hardware checkpoint findings were reconciled against the frozen Stage 14C design and the accepted Stage 14B/Lap Race behaviour.

* Endurance resume now schedules the same five-red-light/start-boundary sequence used by Lap Race resume. Paused duration remains frozen until GO; the Race Engine receives the future Resume Relevant Time and ignores crossings before it.
* Starting presentation shows the configured Endurance duration with “starts at GO”; authoritative duration accounting still begins at GO.
* Frozen Endurance Minutes and Finish controls are visibly disabled while an active Session Definition is running.
* Endurance Finished and Results presentation no longer invents a Lap Race winning time. History now reconstructs Endurance duration, finish policy, laps/classified laps and overtime.
* History result storage is heap-backed at the HTTP boundary; no CompletedRaceResult-sized local is placed on the httpd stack. Results serialization is incremental and bounded.
* The Stage 14C acceptance fixture no longer overlays a live fixture object with its repeated-cycle fixture. It completes result/history checks, destroys the first fixture, then reuses its storage safely.

Record era behaviour was unchanged. Era remains the persisted Record Store generation used by the existing clear/reset operations; PB and Track Record eligibility and reconstruction continue to use the accepted Stage 13/14A semantics.

The final Wokwi acceptance serial evidence contains passing 14C.1–14C.28 and 14C.T results. The final build and resource manifests bind the merged images and measured resource usage to the current source tree.

Fresh cumulative regression evidence is retained in `regressions/stage1-13-manifest.json` with all thirteen campaigns PASS; Stage 14A and 14B evaluator serial proofs are retained beside it. The final acceptance run also removes the temporary repeated-fixture destruction overlap before emitting `ACC DONE`.
