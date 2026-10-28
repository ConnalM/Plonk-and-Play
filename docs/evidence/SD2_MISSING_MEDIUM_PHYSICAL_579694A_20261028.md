# SD-2 missing-medium validation — 579694a

Date: 2026-10-28  
Implementation: `579694a0443fd5d5c26779ddf2216e03d6b96c84`  
Environment: `stage8bp1s3sd`  
Framework: espressif32 8.0.0 / Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5 / Arduino_GFX 1.6.5  
Artifact: `_approved-artifacts/579694a0443fd5d5c26779ddf2216e03d6b96c84/stage8bp1s3sd-arduino33-idf55/firmware.bin`  
Artifact SHA-256: `BDA68390FBFE9ADFE4E3621F3CF5DFFC73792880EF488AF8335DB207D270F184`  
Artifact size: 1,439,696 bytes

## Scope and method

The SD card was physically out for the entire checkpoint. The preserved direct COM6 capture was owned by the dedicated serial-capture process; no computer-use helper owned or probed COM6. No SD reinsertion, recovery, reset, race, write, repair, migration, or second storage request was performed.

Capture: `firmware/logs/device-monitor-sd2-health-579694a-20261028.log`

The only storage consumer request was one read-only `GET /history`, after reachability had been established with `/health`.

## Observed boot and owner state

The capture contains the ESP-ROM boot marker and the approved runtime. Physical SD mount failed at startup:

```text
[P8-P1] SD owner mount begin cs=10 mosi=11 sck=12 miso=13
[P8-P1] SD owner mount FAILED
[P8-P1] SD mount FAILED
...
[P8-P1] SD owner mount begin cs=10 mosi=11 sck=12 miso=13
...
[P8-P1] SD owner mount FAILED
...
[INIT] Phase 8B-P1 SD owner DEGRADED; History storage boundary DEGRADED (namespace reconstruction lazy); NVS configuration READY=YES
[INIT] Normal diagnostics DEGRADED storage=3
```

The later mount attempt is part of the existing lifecycle/startup path; no autonomous retry storm was observed. Browser/AP/STA/mDNS/HTTP startup completed, and the controller remained alive. The periodic legacy `BOOT_SELF_TEST=PASS` marker continued to print, but `/health` exposed the authoritative degraded storage state.

## Reachability and single `/history` request

The working controller route was `http://192.168.68.102`.

`GET /health` returned in the bounded request path:

```http
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 72

{"ok":true,"browser":"stage13","storage":"FAULT","storageDurable":false}
```

Exactly one read-only `GET /history` was then issued. It completed in 0.256893 seconds:

```http
HTTP/1.1 200 OK
Content-Type: application/json
Transfer-Encoding: chunked

{"entries":[]}
HTTP_CODE=200
TIME_TOTAL=0.256893
```

No further storage request was made.

## Result

**Owner/health propagation: observed PASS.** The absent-medium boot state was advertised as owner degraded and non-durable by the initialization diagnostics and `/health`; diagnostics also reported degraded storage.

**Requested missing-medium consumer checkpoint: FAIL / not exercised.** The `/history` route was reached, but because the History namespace remained lazy/empty after boot mount failure, it returned a successful empty list without opening a selected payload. Consequently this request did not trigger the proven filesystem read/open failure boundary, so this run cannot demonstrate an explicit `/history` storage error or a consumer-to-owner transition. It must not be treated as proof that the missing-medium read path is fixed.

No panic, watchdog, reset loop, persistence fault, fabricated successful write, race mutation, authority mutation, or Browser/SSE instability was observed. No consumer-side `SD.begin()`, independent remount, or autonomous recovery storm was observed. The SD card remains out and owner recovery was not invoked.

This evidence is preserved for Archie’s review; no further physical action is requested.
