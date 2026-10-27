# HTTP/SSE reconnect validation — 8090794

Date: 2026-10-27

## Candidate

- Source commit: `8090794f134713e5c7e21943f1dbc329491680a5`
- Environment: `stage8bp1s3sd`
- Preserved artifact: `_approved-artifacts/8090794f134713e5c7e21943f1dbc329491680a5/stage8bp1s3sd/firmware.bin`
- Artifact size: `1465600` bytes
- Artifact SHA-256: `CB565DAEC7A644B47DE7BAB0D0199B85CA3E08B179918F8F33FB32B40380A53A`
- Runtime `/health` provenance matched the source commit and environment; ELF prefix reported `96cd1c942`.

## Physical run

Fresh COM7 capture: `firmware/logs/device-monitor-8090794-physical.log`.

Two admitted SSE sessions were observed. The controller-side context evidence identified the tablet as Master (`client_tag=3`, `is_master=true`) and the laptop as spectator (`client_tag=2`, `is_master=false`). The controller remained alive, emitted regular SD self-test PASS and SSE keepalive output, and no panic/reset/race/persistence activity occurred.

The latest pre-loss population observed during Browser synchronisation was 11 HTTPD clients. After the tablet was disconnected, the first complete failure boundary was:

```
[HTTP_ADMISSION] route=/ fd=63 outcome=ordinary_capacity_exhausted clients=13 sse_slots=0 page_spaces=2 server_generation=1
```

This violates the required no-progressive-client-accumulation outcome for the reconnect regression. The run was stopped immediately; the tablet was not reconnected, no Browser was refreshed, and no controller reboot or retry was performed. The reboot-recovery test was not started.

The capture does not establish a P&P SSE-session leak: both existing workers continued to report keepalive, and no new SSE admission or panic was observed before the capacity rejection. The non-clean starting population (11 clients) is retained as a test-environment qualification issue for the next attempt rather than a basis for a firmware correction.

