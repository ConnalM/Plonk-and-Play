# Framework migration display AP-only discriminator

Date: 2026-10-26  
Branch: `codex/history-read-diagnostic`  
Candidate: `f9e7df8b18163a8b7464bb0ae656ecf3709f722e`  
Environment: `stage8bp1s3sd-display-ap-only-stop`  
Firmware SHA-256: `4FD4BA6C4D2ABA512A31675C5842514DA86919519FDDD18E1768A1EF17B6B5DE`

## Physical result

Capture: `firmware/logs/device-monitor-261026-211312.log`.

The full-P&P diagnostic reached the AP-only boundary:

```
[DISPLAY_ORDER_PROBE] boundary=before_display_begin internal_free=275128 internal_largest=212980 psram_free=8384788 psram_largest=8257524
[DISPLAY] ESP32-8048S070 init OK
[DISPLAY] P&P READY FOR WORK
[DISPLAY_ORDER_PROBE] boundary=after_display_ready internal_free=272296 internal_largest=212980 psram_free=7616400 psram_largest=7602164
[P8-P1] SD owner mount OK
[P8-P1] History store begin owner_lease=1 owner_mounted=1 owner_health=1
[WIFI] STA_EVENT event=START
[WIFI] AP_ONLY ap_started=1
[DISPLAY_ORDER_PROBE] boundary=ap_only internal_free=226240 internal_largest=180212 psram_free=7574912 psram_largest=7471092
[DISPLAY_AP_ONLY_STOP] startup halted after AP creation before STA begin, mDNS and first browser.tick
```

The physical observation was **READY corrupted intermittently**.  The
corruption was present despite withholding remembered-STA `WiFi.begin()` and
association, mDNS startup, Browser tick and HTTP/SSE startup.  No panic or
reset occurred in the capture.

## Interpretation

This clears STA association, mDNS and HTTP/SSE as necessary causes.  The
remaining boundary is the synchronous/asynchronous Wi-Fi mode/event/AP path:

* `WiFi.persistent(true)`;
* `WiFi.mode(WIFI_AP_STA)` and its ESP-IDF Wi-Fi/netif/event infrastructure;
* P&P event callback registration;
* `WiFi.softAP()` and AP startup.

Because the corruption is intermittent, the result does not identify which of
those sub-operations first perturbs RGB scanout or its DMA/cache environment.
It does establish that the later STA and mDNS branches are not required.

The AP-only heap snapshot remains non-exhausted: internal free 226,240 bytes
(largest block 180,212) and PSRAM free 7,574,912 bytes (largest block
7,471,092).

No production display, Wi-Fi or network semantics were changed.
