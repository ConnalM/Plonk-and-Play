# P&P Gates G1–G4 software-closure evidence

**Date:** 23 October 2026  
**Branch:** `browser-product-ux-stage15`  
**Plan baseline:** `eb476316e1d48a889925a52a40f0df4cdf7c91cb`

This record is a technical handover for Archie’s Gate A review. It records
the closure work performed against `docs/MASTER_PROJECT_PLAN.md`; it does not
declare product acceptance or replace the plan.

## WP1 / G1 — racing core

The current production-path Stage 14C facilities cover all three frozen Lap
finish policies. The committed Stage 14C serial evidence records 14C.P16
(immediate/all stop), 14C.P17 (complete current lap), and 14C.P18 (complete
full race distance), together with the reversed-delivery, one-crossing and
no-origin regressions. The Browser proposal, start presentation, pause/resume,
termination controls, Endurance and Open Practice runtime checks pass.

The current host checks pass for the Stage 14C structural review, evaluator
integrity, presentation corrections, all Browser runtime scripts and the
practice-summary lifecycle. The full Wokwi campaign was not rerun because the
configured Wokwi account reported its monthly Hobby Plus minutes exhausted;
the committed campaign evidence remains valid and is credited under the
one-plan evidence rule.

The former `latest_lap_is_authoritative_cross_entry` check was stale: it
expected the removed `entry.records[...]` dereference. It now asserts the safe
authoritative `lastCrossing` path and the absence of that dereference.

## WP2 / G2 — Results and persistence

The persistent-storage runner passes PSF.1–PSF.30, WP2B, WP2C, WP2D, WP2E and
WP2F, including reconstruction, active-detail streaming, transactional
failure boundaries, rolling retention and the physical SD ownership contract
tests PSF.31–PSF.37 / PS15.88–PS15.92. The Results, Details, History and
Practice Browser regressions pass.

The historical `1 lap` / no-time observation is a Results defect: a competitor
stopped immediately when the winner finishes still has an authoritative elapsed
duration for its completed progress, so Results TIME must show that duration.
Only an entry with no completed lap has TIME `—`. The serializer supplies
per-entry `finishDurationUs`; the Browser never substitutes overall `finishTime`
or calculates a duration. A fresh physical Results/Details/History check remains
part of WP5.

The Results HTTP stack correction is present and the Browser Results runtime
regression exercises current, historical and unavailable-result paths without
the former detail dereference.

## WP3 / G3 — Browser, authority and networking

The accepted AP+STA, Scan/Connect/Disconnect/Forget, LAN, mDNS and diagnostics
network evidence remains credited by the master plan. The Stage 15 contract
and phase runners pass, including network, control lease, Browser recovery,
observation, export, production recorder, durable sequence and bounded
multi-client SSE model checks.

The retained Race Director correction is now ordered so that the production
authority store is opened first, then the persisted master fingerprint is
restored exactly once before HTTP requests can be served. The constructor no
longer queries an unopened store. `run_control_lease.ps1` now guards this
ordering and the existing control-lease unit test continues to pass.

The production multi-client HTTP/SSE choreography runner reports
`NOT EXECUTED` when `PP_MULTI_CLIENT_BASE_URL` is absent. This is an execution
environment limitation, not a changed requirement; physical multi-client and
reboot proof remains WP5.

## WP4 / G4 — diagnostics and support

The Stage 15 diagnostic runners pass for recorder phases 1–7, privacy and
retrieval contracts, WP3 production/export/sequence checks and WP4A
durability. The persistent-storage suite confirms bounded/degraded failure
isolation. The accepted real export evidence records complete, ordered,
non-duplicated records with provenance and no evidence-incomplete flag.

The support path remains observation-only: diagnostic failures cannot change
Race Engine authority or racing state. Physical SD failure/degraded-storage
behaviour and the final real-hardware support path remain WP5 obligations.

## Software candidate build

The clean `stage8bp1s3sd` build completed with the installed PlatformIO
Espressif32 6.9.0 toolchain. The merged ESP32-S3 image is:

`firmware/.pio/build/stage8bp1s3sd/firmware.merged.bin`

Size: 1,099,472 bytes  
SHA-256: `C6FD7218BEA1137C747FC2E0DB4CF37820520C9EF2C374D364E095AFD9395A62`

The merged image was regenerated after the source change. Header/offset checks
show bootloader image header `E9` at `0x0000`, partition-table magic `AA` at
`0x8000`, `boot_app0.bin` at `0xE000`, and application image header `E9` at
`0x10000`.

## Review position

No new product or scope decision was required. Remaining items are physical
WP5 evidence (including SD recovery/degraded operation, the three fresh
finish-policy checks, multi-client/reboot authority proof and other appliance
coexistence checks), plus the unavailable Wokwi rerun and host production
SSE choreography where their environments are available. This record is ready
for Archie’s cross-work-package semantic review; it does not silently promote
those physical obligations into software defects.
