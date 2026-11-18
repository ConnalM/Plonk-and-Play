# Final review package contents — 18 November 2026

This package is for independent checkpoint review only. It makes no Git
history change and does not replace the physically accepted firmware image
`3649CEE0EE12EF4630EFD5A0825B57EB37B7C3603543E7A9093AA540691D1A08`.

## Decision reconciliation

- Shared configuration accepts a positive 32-bit lap target and migration must
  retain a historically valid target and unrelated customer settings.
- A Browser-controlled race may use that shared range.
- A standalone Taster must reject START when its displayed Lap Race target is
  above 999. It must require an explicit local distance adjustment, which
  re-enters the Taster's 1–999 range at 999. It must never silently rewrite the
  shared value just to enable START.
- The existing Race Engine calculation `lapTarget() + penalties_` can overflow
  in 32-bit arithmetic at an extreme valid target. It is recorded as an open
  defect; no corrective source is included in this package.
- SMUGLAND Browser visual design and acceptance is an explicit open WP8
  workstream owned by Gina. It is separate from current functional Browser
  contracts.

## Included evidence

The package contains the master plan, authoritative Taster/configuration,
product-facilities and common-display specifications; the full compliance and
checkpoint reconciliations; the standalone-record plan; Race Engine design;
and the reproducible XIP build reference. It contains the verified migration
source patch `Archie_Configuration_Migration_Source_2026-11-18.patch` (SHA-256
`1D522EFF23D29C1368B1051870B85A200E866416BE6D05AA474180EE4F27F61D`).

## Proposed controlled checkpoint contents

Do not stage indiscriminately. Select reviewed hunks only in pre-existing
dirty files. The checkpoint review must include:

- Migration correction source and tests listed in
  `ARCHIE_CHECKPOINT_REVIEW_2026-11-18.md`.
- The governing documentation listed in that same review, including this
  final-contents document.
- The separately reviewed WP8 presentation, approved artwork and build-input
  slices listed under “Candidate files”.

The standalone-only Taster START gate is implemented in
`firmware/include/pp/taster_surface.h` and accepted by focused Taster and
Browser host regressions. Its separate source diff is included as
`Archie_Standalone_Start_Gate_Source_2026-11-18.patch`. The accepted firmware
must remain the stated 3649CEE0 recovery checkpoint until a separately
authorised build, flash and physical acceptance occur. Exclude build output,
caches, credentials, recovery images and temporary diagnostics.
