# HTTP/SSE tablet Wi-Fi restoration — 8090794

Date: 2026-10-27

Capture: `firmware/logs/device-monitor-8090794-tablet-restore.log`.

## Pre-restoration baseline

Before the single physical action, the controller was healthy with two SSE workers (`generation=4` and `generation=5`) reporting keepalive. Read-only `/health` and `/context` completed successfully at HTTPD population 13. The laptop context was spectator and the retained tablet authority was Master.

## Automatic recovery

After the tablet Wi-Fi was switched back on, with the P&P page untouched:

1. A new `/events` request was admitted at `clients=8`, `sse_slots=1`; no premature rejection occurred.
2. The prior tablet worker was replaced and a new worker was created (`generation=6`).
3. Fresh `/state`, `/proposal`, and `/context` requests completed before the tablet was considered synchronised.
4. `/context` reconstructed the tablet as Master (`client_tag=2`, `is_master=true`, `owner_connected=true`).
5. The laptop SSE (`generation=4`) continued keepalives throughout.
6. The topology settled with two SSE workers and no progressive client-count growth, panic, reset, authority mutation, race, or persistence activity.

There were two short-lived `/` page-send failures during the reconnection interval (`result=45062`, 318 ms and 237 ms), but they did not prevent SSE admission, fresh authoritative acquisition, or stable two-client operation. No controller reboot-recovery test was started.

