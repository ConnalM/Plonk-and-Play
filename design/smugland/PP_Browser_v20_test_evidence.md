# V20 auxiliary Integrated Display — design review evidence

Candidate: `30f2ea5334a2aa027dbcb2f22bd7a44cb0d2b183`  
Review: issue #2, comment 7037518766. Design branch only.

## Tested
Headless Chromium / Playwright, local HTML loaded in-memory. 128 passing assertions, comprising 62 auxiliary/lifecycle/storage checks and 66 navigation/authority/layout assertions (11 per width per version, V18 and V20 at 1440, 768, 390 px).

- Fixed/manual factory start; protected factory profile; actual 1–8 lane counts; independent review-only mock lane overrides.
- Separate draft/applied state; Save, Save As, Rename, Delete, load and confirmation/cancel paths; auxiliary restore leaves standalone, installation and other configurations intact; race-type association applied at parent mode selection.
- Race remains authoritative during editing/applying; editor instance and unsaved edits survive race ticks and navigation; role loss removes editor access and rejects edits.
- Parent countdown/GO (Lights Out/Green), pause/resume, optional Practice countdown, Practice summary without ranks/winner.
- Running screen and lane page timers; actual winner across automatic finish pages, including across Browser's existing Results handoff; tie and next setup; one-lane Endurance finish.
- Dedicated Drag Tree and Timed Stage read from existing authoritative simulated parent fixtures, which remain specialist review-only modes.
- Baseline V18 and V20 navigation: Race, Results, Track Setup, Racers and Settings; sound, Browser display, standalone, Advanced; spectator restriction, horizontal overflow and JS-error checks at each viewport.

## Evidence limits
Native-origin navigation/localStorage was blocked by the Chromium test sandbox. Persistence and reload/fallback paths exercised using an explicit test-only storage stub; storage-unavailable memory-only fallback checked without stub. No controller/SD/physical display test; no new race engine, production or firmware changes. Hardware feasibility (dynamic rendering, 1–8 lanes and SD storage) remains unconfirmed. This is a review candidate, not production acceptance. V18 comparison tests are representative regression checks, not an exhaustive retest of every prior fixture.

## Review
Open Settings → Integrated Display. The auxiliary editor is below preserved standalone settings. The review-only applied physical Taster preview can be expanded independently of routing. Restore Auxiliary Display Defaults applies the built-in factory configuration and retains named custom configurations; Save and Apply remain separate. Specialist fixtures and start sequence selectors are explicitly review-only. Race-affecting settings remain locked while active; Settings navigation and authorised auxiliary display edits remain available.
