# Full-P&P display post-early-stop analysis

## Physical evidence

The full-image early-stop candidate `c4fb211b7835ffa08d297dde8e1ee403eba00b30`
was physically validated without starting SD, Browser/network or HTTP. The
READY screen was clean and steady. Capture:
`firmware/logs/device-monitor-261026-202936.log`.

The capture contains successful display initialization, both heap boundaries,
and:

`[DISPLAY_FULL_IMAGE_EARLY_STOP] startup halted after READY draw status=READY`

| boundary | internal free | largest internal block | PSRAM free | largest PSRAM block |
|---|---:|---:|---:|---:|
| before display begin | 301,100 | 241,652 | 8,384,788 | 8,257,524 |
| after READY draw | 298,268 | 241,652 | 7,616,400 | 7,602,164 |

This proves the full linked image/global object set can produce a stable READY
frame. It does not rule out later startup; it moves the required boundary to
operations after the early draw.

## Ordered operations after the early draw

`setup()` proceeds in this order:

1. `storage.begin()` opens NVS Preferences; Browser authority Preferences are
   opened and retained master authority is restored.
2. `PhysicalSdOwner::setLifecycle()` and `PhysicalSdOwner::begin()` create/take
   the recursive owner mutex, call `SPIClass::begin()` on the SD pins, call
   `SD.begin()`/FATFS mount, and record mount health.
3. `Esp32SpiSdStore::begin()` takes the owner lease, creates/checks `/pp-p1`,
   removes stale transaction files, and queries card capacity/usage. History,
   detail and Track Record stores remain logical users of this owner.
4. The SD-backed diagnostic recorder starts, taking the same owner lease,
   creating/checking its namespace/file and scanning or recovering existing
   records.
5. Browser diagnostic provenance is attached, fixture-only callbacks are
   conditionally installed, and fixed Message Bus subscriptions are registered.
   These are primarily fixed in-memory setup; they do not start Wi-Fi or HTTP.
6. `browser.begin()` calls `ExternalWifiManager::begin()`: persistent AP+STA
   mode is enabled, Wi-Fi event callbacks are installed, the P&P AP starts,
   remembered driver-owned STA configuration is detected and association starts
   asynchronously when present, and mDNS is started.
7. On the first `loop()`, `browser.tick()` services AP/STA state and starts the
   HTTP server. `httpd_start()` creates the HTTP task, socket pool and routes;
   SSE sessions are created only after clients request `/events`.
8. The remaining path starts optional Taster support when compiled, runs the
   bus self-test, publishes the configuration-load request, and enters normal
   Race Engine/Browser/diagnostic servicing.

The plausible post-display resource/peripheral boundaries are therefore SD
SPI/FAT mount, SD-backed diagnostic file operations, Wi-Fi AP/STA driver
startup, and HTTPD task/socket creation. No later source path intentionally
frees or reconfigures the RGB framebuffer.

## Smallest next breakpoint

The best midpoint is immediately before `browser.begin()`, after NVS,
PhysicalSdOwner/SD mount, History namespace setup, diagnostic-recorder startup,
Browser diagnostic wiring, and Message Bus subscriptions. A diagnostic-only
variant preserving that ordering, then drawing only the existing READY frame and
idling before `browser.begin()`, divides the sequence into storage/pre-network
and Wi-Fi/HTTP halves:

- READY corrupt there: the SD/SPI/FAT or diagnostic-storage half is implicated.
- READY remains clean: the next boundary is `ExternalWifiManager::begin()` and
  first-loop HTTP startup.

This is a proposed breakpoint only. No firmware was built or changed for this
analysis, and no race or HTTP test was performed.
