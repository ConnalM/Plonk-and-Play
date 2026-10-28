# Two-browser RESET/EN recovery acceptance

Date: 2026-10-28

## Candidate

- Source: `230f1ba09465f3751aa9916624b78848f2457925`
- Environment: `stage8bp1s3sd`
- Artifact SHA-256: `8387FDB7C85AED280E6CA9A18432B292226B12E7B6FA640F43DE2077FC69BFEC`
- Serial capture: `firmware/logs/device-monitor-reset-recovery-230f1ba-final-20261028.log`

## Result

**PASS — automatic two-browser RESET/EN recovery.**

The dedicated COM6 capture was the sole serial owner. A single RESET/EN action produced the ESP-ROM boot sequence. The post-reset boot reported SD mount OK, AP start, remembered STA association, mDNS start, HTTP server start, display init OK, and the approved source/environment identity.

## Recovery evidence

- Browser A: `client_tag=1`, `is_master=false`, spectator.
- Browser B: `client_tag=2`, `is_master=true`, Race Director.
- Both browsers completed fresh sequential `/state`, `/proposal`, `/context` acquisition.
- SSE generation 1 (`fd=51`) and generation 2 (`fd=55`) were admitted and continued keepalives.
- HTTP client population rose during acquisition to 8, below the protected 9-client rejection boundary, then settled without progressive growth.
- No `ordinary_capacity_exhausted`, SSE `send_failure`, panic, reset loop, race mutation, persistence fault, or authority mutation occurred after the deliberate reset.
- The final observation window contained healthy keepalives from both SSE generations.

The complete raw capture is retained at the path above.
