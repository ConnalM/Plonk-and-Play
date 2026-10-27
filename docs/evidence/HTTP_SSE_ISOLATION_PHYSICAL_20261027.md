# HTTP/SSE isolation physical validation — FAIL

Date: 2026-10-27  
Candidate: `c20a45211fcc46681d438ad44dc3d4636dd41eed`  
Environment: `stage8bp1s3sd`  
Application image SHA-256: `B4CD1F661B9473E04BCFA0A87DCAE43717242A1B4604028143DC5F70B09AF096`  
ELF SHA prefix reported at runtime: `9dece6bcb`

Capture: `firmware/logs/device-monitor-261027-100733.log`.

## Setup

The normal image booted successfully. The capture showed SD owner mount OK,
healthy History owner state, AP `192.168.4.1`, remembered STA association and
GOT_IP `192.168.68.102`, mDNS startup, HTTP server startup, clean RGB display
initialisation and the exact c20a452 build identity. No race was run.

The test then held one root-page TCP client and one `/events` TCP client open
without reading either response. A separate healthy client requested
`/health`, `/context`, `/state`, `/proposal` and opened a second SSE stream.
The stalled sockets were held for 15 seconds and then closed once the evidence
had been collected.

## Observed result

Healthy-client measurements while both stalled clients were held:

| Request | Result |
|---|---|
| `/health` | timed out at 10.034 s |
| `/context` | HTTP 200, 261 ms |
| `/state` | HTTP 200, 80 ms |
| `/proposal` | HTTP 200, 87 ms |
| healthy `/events` | connection forcibly closed, 4.988 s read timeout |

After the stalled sockets were closed, `/health` returned HTTP 200 in 200 ms
and `/context` returned HTTP 200 in 94 ms.

The Serial capture contains the first proven failure boundary:

```text
[HTTP] ERROR route=/normal duration_ms=5549 result=45062
Guru Meditation Error: Core 0 panic'ed
Debug exception reason: Stack canary watchpoint triggered (pp_sse_0)
Rebooting...
...
Guru Meditation Error: Core 0 panic'ed
Debug exception reason: Stack canary watchpoint triggered (pp_sse_0)
Rebooting...
```

The controller therefore rebooted during the isolation test. The failure is
not a passing isolation result: a dedicated SSE worker's 4,096-byte stack was
exhausted while transport pressure was present. The preceding `/normal`
`ESP_ERR_HTTPD_RESP_SEND` and the healthy SSE disconnect are retained as
secondary transport evidence. No authority, race or persistence operation was
performed by the test.

## Classification

**FAIL — stop and review before any patch or repeat test.** The async design
did isolate enough work for `/context`, `/state` and `/proposal` to remain
responsive, but it did not meet the required no-panic/no-reboot and healthy-SSE
criteria. No speculative correction has been made.

