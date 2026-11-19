# WP8 Shared Simulation and Physical-Taster Acceptance

**Status:** Acceptance-only facility. It is compiled only when the isolated
XIP build is configured with `PP_WP7_ACCEPTANCE_SIMULATION=1`; it is absent
from normal and customer firmware.

## Boundary

All four detector origins terminate at the existing Input Module and are
consumed by the existing Race Engine, Race Control, Message Bus, Noticeboard,
Taster presentation and Browser projection. There is no second racing loop.

| Origin | Current use | Record policy |
|---|---|---|
| Physical detector | Normal racing | Existing frozen source/eligibility policy |
| Manual SMUG simulation | Existing controller-authorised `fixture` crossing | `simulatedSession` is armed before START; never official |
| Automated Demo | Reserved future origin only; no customer feature in this build | Must arm `simulatedSession`; never official |
| PC acceptance script | This opt-in image, via the same fixture crossing | `simulatedSession` is armed before START; never official |

`simulatedSession` is frozen into `SessionDefinition`. The Race Engine gates
History, Personal Bests, Track Records and persistent race details on that
frozen flag. A simulated session may remain live on the Noticeboard, Taster
and Browser, but cannot later become official through SMUG connection or
control handover.

## Standalone Taster bridge

On the acceptance image only, `/fixture` accepts `taster-setup`,
`taster-arm`, `taster-start`, `taster-pause`, `taster-resume`,
`taster-lane1`, `taster-lane2` and `taster-reset` while no controlling Browser
is connected. The bridge calls `Wp7PhysicalRacingUi` for setup/start/pause/
resume, so requests retain the registered `StandaloneTaster` endpoint and
Race Control's ordinary authority decision. Crossings remain the existing
momentary simulated Input Module sources. A connected Race Director blocks
every `taster-*` action, just as it blocks a physical local action.

Sequences and assertions are external:

- `acceptance/stage8/physical_taster_simulation_acceptance.py` exercises
  standalone setup, START, PAUSE/RESUME, two-lane finish, Browser-visible
  result and record snapshots.
- `acceptance/stage13/http_acceptance.py` is the revised controller/SSE
  simulated-race acceptance.
- `acceptance/stage15/multiclient_http_acceptance.py` is the revised
  multi-client authority/SSE acceptance.

All scripts require an explicit authorised URL and `--acceptance`. They do
not erase records. The physical test operator must still witness the Taster
screen during each generated state; remote Browser state proves projection,
not pixels.

## Acceptance coverage and limits

The shared host journey verifies simulated session freezing, normal timing,
finish settlement and record exclusion. The external Taster journey adds
standalone authority and pause/resume. Stage 13/15 cover controller authority,
Browser convergence, SSE replacement and Browser-visible result/detail data.
The image is only prepared in this checkpoint: it must not be flashed or run
on the physical Taster without a separately authorised physical test.
