# Framework migration display investigation: post-`browser.begin()` boundary

Date: 2026-10-26  
Branch: `codex/history-read-diagnostic`  
Candidate: `0545f8334c5b2917c67179842f83d2892845523e`  
Environment: `stage8bp1s3sd-display-post-browser-begin-stop`

## Physical evidence

Capture: `firmware/logs/device-monitor-261026-205502.log`.

The approved post-`browser.begin()` / pre-first-tick candidate booted with the
expected display-order markers:

```
[DISPLAY_ORDER_PROBE] boundary=before_display_begin internal_free=272680 internal_largest=212980 psram_free=8384788 psram_largest=8257524
[DISPLAY] ESP32-8048S070 init OK
[DISPLAY] P&P READY FOR WORK
[DISPLAY_ORDER_PROBE] boundary=after_display_ready internal_free=269848 internal_largest=212980 psram_free=7616400 psram_largest=7602164
[P8-P1] SD owner mount OK
[P8-P1] History store begin owner_lease=1 owner_mounted=1 owner_health=1
[WIFI] STA_EVENT event=START
[WIFI] mDNS STARTED host=plonk-and-play.local
[WIFI] P&P AP STARTED ssid=Plonk-and-Play ip=192.168.4.1 clients=0
[DISPLAY_ORDER_PROBE] boundary=post_browser_begin internal_free=217696 internal_largest=172020 psram_free=7574732 psram_largest=7471092
[DISPLAY_POST_BROWSER_BEGIN_STOP] startup halted after browser.begin before first browser.tick
[WIFI] STA_EVENT event=CONNECTED channel=3 authmode=3
[WIFI] STA_EVENT event=GOT_IP ip=192.168.68.102
```

The physical result was **CORRUPTED READY text**.  The display was clean in the
pre-`browser.begin()` probe, so this is a valid boundary result: the trigger is
inside `browser.begin()` or asynchronous work it starts.  `browser.tick()` did
not execute, and there is no HTTP/SSE server-start marker in this capture.

Heap deltas are substantial but not exhaustion by themselves:

| boundary | internal free | largest internal | PSRAM free | largest PSRAM |
|---|---:|---:|---:|---:|
| before display | 272,680 | 212,980 | 8,384,788 | 8,257,524 |
| after READY | 269,848 | 212,980 | 7,616,400 | 7,602,164 |
| after `browser.begin()` | 217,696 | 172,020 | 7,574,732 | 7,471,092 |

The later STA association and DHCP events occur after the stop marker because
association is asynchronous.  They confirm that the Wi-Fi driver remains
active while the application is idling; they do not prove that association,
rather than AP startup or mDNS, is the first corrupting operation.

## Synchronous call sequence

`firmware/src/main.cpp:767` calls `browser.begin()`.  In the real-SD build,
`firmware/include/pp/browser_interface.h:78-84` calls only
`network_.begin()`, records AP state and marks the Browser interface started.
HTTP/SSE setup is later in `BrowserInterface::tick()` and is therefore outside
this probe.

`firmware/include/pp/external_wifi.h:93-117` performs the following sequence:

1. `WiFi.persistent(true)` selects driver-managed NVS persistence.
2. `WiFi.mode(WIFI_AP_STA)` performs Arduino-ESP32 low-level Wi-Fi/netif
   initialization, starts the ESP-IDF Wi-Fi driver and enables AP/STA
   interfaces.  The framework creates/starts its Wi-Fi/event infrastructure;
   P&P does not create a new task here.
3. `installEventDiagnostics()` registers the P&P event callback.
4. `WiFi.softAP("Plonk-and-Play")` enables the AP netif, DHCP/AP services and
   begins AP operation.
5. `esp_wifi_get_config(WIFI_IF_STA, ...)` checks the driver-owned stored STA
   SSID.  With a remembered configuration, `WiFi.setAutoReconnect(true)` and
   `WiFi.begin()` call the Arduino STA path.  The framework enables the STA
   netif, waits for it to start, reapplies the stored config, configures DHCP,
   and calls `esp_wifi_connect()`; association then proceeds asynchronously.
6. `startMdns()` calls `MDNS.begin("plonk-and-play")` and adds the HTTP service.
   This initializes the IDF mDNS responder; it is not the HTTP server.

The observed `STA_EVENT START` interleaving is asynchronous framework/event
work, so Serial order does not establish a different synchronous source order.

## What is and is not established

Established:

* storage/SD/SPI/FAT/diagnostic startup is cleared by the clean
  pre-`browser.begin()` physical result;
* the failure is introduced by the network startup boundary before first
  Browser tick;
* HTTP/SSE and Browser request handling are not required to reproduce it;
* the migrated Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5 framework path is executing
  the AP+STA and mDNS startup calls shown above;
* free and largest internal/PSRAM blocks remain available after startup, so
  heap exhaustion alone is not proven.

Not established:

* whether the first corrupting transition is `WiFi.mode`, AP creation, STA
  startup/association, or mDNS;
* whether Wi-Fi driver activity changes RGB DMA/cache behaviour, or whether a
  particular internal allocation or peripheral interaction is responsible;
* any HTTP/SSE contribution (the server never started in this probe).

No display settings, timing, pins, framebuffer construction, Wi-Fi semantics,
or production code were changed for this evidence.

## Smallest next discriminator

Use one disposable full-P&P diagnostic variant that retains the same display,
storage and framework configuration but stops inside the network boundary
immediately after AP creation and before `WiFi.begin()`/mDNS.  If READY remains
clean, add the remembered-STA `WiFi.begin()` path; if it then corrupts, the
STA start/association path is isolated.  If AP-only already corrupts, the
boundary is `WiFi.mode`/AP startup.  mDNS can be enabled as the final isolated
step only if both Wi-Fi branches remain clean.

This is a diagnostic split, not a production design or a display-setting
change.  No new physical test is requested by this document.
