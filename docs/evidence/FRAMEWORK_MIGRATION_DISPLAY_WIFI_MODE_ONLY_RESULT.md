# Framework migration display WiFi-mode-only discriminator

Date: 2026-10-26  
Branch: `codex/history-read-diagnostic`  
Candidate: `40fbd8d34628a8de155747989027e824c3834676`  
Environment: `stage8bp1s3sd-display-wifi-mode-only-stop`  
Firmware SHA-256: `E62AE6901CD800CA605605D48B77083D691EAE8A70345B27F12243A34AABF3C9`

## Physical result

Capture: `firmware/logs/device-monitor-261026-213113.log`.

The full-P&P diagnostic reached the WiFi-mode-only boundary:

```
[DISPLAY_ORDER_PROBE] boundary=before_display_begin internal_free=275128 internal_largest=212980 psram_free=8384788 psram_largest=8257524
[DISPLAY] ESP32-8048S070 init OK
[DISPLAY] P&P READY FOR WORK
[DISPLAY_ORDER_PROBE] boundary=after_display_ready internal_free=272296 internal_largest=212980 psram_free=7616400 psram_largest=7602164
[P8-P1] SD owner mount OK
[P8-P1] History store begin owner_lease=1 owner_mounted=1 owner_health=1
[WIFI] WIFI_MODE_ONLY mode=WIFI_AP_STA returned
[DISPLAY_ORDER_PROBE] boundary=wifi_mode_only internal_free=226376 internal_largest=180212 psram_free=7575072 psram_largest=7471092
[DISPLAY_WIFI_MODE_ONLY_STOP] startup halted after WiFi.mode(WIFI_AP_STA) before event registration and AP creation
```

The physical observation after approximately 20 seconds was **the same
intermittent READY corruption** seen in the AP-only probe.  No P&P event
callback registration, softAP startup, remembered-STA association, mDNS,
Browser tick or HTTP/SSE startup occurred.  No panic or reset occurred.

## Interpretation

The AP, STA, mDNS and HTTP branches are cleared.  The remaining implicated
boundary is `WiFi.persistent(true)` / `WiFi.mode(WIFI_AP_STA)` and the
Arduino-ESP32/ESP-IDF WiFi driver/netif infrastructure initialized by that
mode call.  The P&P event callback and AP creation are not required to produce
the corruption.

The heap snapshot is not an exhaustion result: internal free is 226,376 bytes
(largest block 180,212), and PSRAM free is 7,575,072 bytes (largest block
7,471,092).

No production display or networking semantics were changed.
