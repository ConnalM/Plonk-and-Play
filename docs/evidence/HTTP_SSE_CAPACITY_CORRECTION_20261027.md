# HTTPD client-capacity admission correction

Implementation commit: `8090794f134713e5c7e21943f1dbc329491680a5`  
Branch: `codex/http-sse-isolation`  
Environment: `stage8bp1s3sd`

## Correction

`BrowserInterface` now has one `HttpdMaxOpenSockets = 13` source used by:

* `httpd_config_t::max_open_sockets`;
* `httpd_get_client_list()` array/capacity arguments;
* page and `/events` admission arithmetic.

The existing `OrdinaryRequestReserve = 4`, four-SSE limit, explicit 503 plus
`Connection: close` rejection, async ownership, page workers, Browser recovery,
authority, race and persistence semantics are unchanged. The incorrect
second subtraction of ESP-IDF's three internal HTTPD sockets is removed.

The resulting boundary is admission through client counts 6, 7 and 8, with
explicit rejection at 9 or above (`13 - 4`).

## Deterministic proof

The Stage 10 deterministic model now covers:

* one healthy SSE session;
* bounded ordinary keep-alive population;
* second-SSE admission at 6/7/8 clients;
* 503/`Connection: close` rejection at 9+;
* no session or async ownership allocation on rejection;
* one terminal close path;
* stale keep-alive reclamation and recovery without mutating the healthy SSE;
* ordinary `/health`, `/context`, `/state`, `/proposal` and healthy-SSE budgets;
* fd/generation rejection, bounded page/SSE admission and exactly-once release.

## Tests

PASS:

* `python acceptance/stage10/http_sse_isolation_contract.py`
* `python acceptance/stage10/http_sse_isolation_deterministic.py`
* `python acceptance/stage10/structural_review.py`
* `powershell -ExecutionPolicy Bypass -File acceptance/stage15/run_sse_multiclient.ps1`
* `powershell -ExecutionPolicy Bypass -File acceptance/stage15/run_control_lease.ps1`
* `powershell -ExecutionPolicy Bypass -File acceptance/stage15/run_wp3_browser.ps1`
* `powershell -ExecutionPolicy Bypass -File acceptance/stage15/run_wp4a_diagnostic.ps1`
* all Stage 14C `browser_*runtime.js` contracts and `structural_review.py`
* `powershell -ExecutionPolicy Bypass -File acceptance/stage15/run_network_contract.ps1`
* `powershell -ExecutionPolicy Bypass -File acceptance/persistent_storage/run.ps1`
* clean PlatformIO build for `stage8bp1s3sd`

The optional live Stage 15 multiclient choreography was not run because no
`PP_MULTI_CLIENT_BASE_URL` was supplied; its structural contract passed.

## Artifact

Preserved artifact:

`_approved-artifacts/8090794f134713e5c7e21943f1dbc329491680a5/stage8bp1s3sd/firmware.bin`

Manifest: `manifest.txt` in the same directory.  The preserved copy was
re-hashed after copying and matches the manifest:

* size: `1,465,600` bytes
* firmware SHA-256: `CB565DAEC7A644B47DE7BAB0D0199B85CA3E08B179918F8F33FB32B40380A53A`
* application ELF SHA-256: `96CD1C9425D0002629174B30A1554C0AF8726D076BCA5163528F6CB791F25900`
* static RAM: `88,884` bytes
* reported application flash: `1,437,307` bytes
* SSE worker stack: `8,192` bytes each, `32,768` bytes for four workers
* framework: Arduino-ESP32 `3.3.12` / ESP-IDF `5.5.5`
* PlatformIO Espressif32 `8.0.0`

No hardware was flashed and no race was run.
