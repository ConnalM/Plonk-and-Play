# Framework migration production RGB bounce-buffer validation

## Candidate

- Source commit: `ab0417261e1009ee8d5f291605a492856410316e`
- PlatformIO environment: `stage8bp1s3sd`
- Application image: `firmware/.pio/build/stage8bp1s3sd/firmware.bin`
- Image size: `1,454,544` bytes
- Image SHA-256: `2088A9101BB1F22DE64DEE71ECADC0536F4E8EF0B2CF60779AC855FDBDBD9CC5`
- Runtime ELF identity: `0354f91ff` (reported by the running application)
- Serial capture: `firmware/logs/device-monitor-261026-223400.log`

## Physical result

The exact normal production image was uploaded to COM7 and hard-reset. The fresh
capture reported:

```text
[DISPLAY] RGB bounce buffers configured: bounce_buffer_size_px=16000 bytes_each=32000 total_bytes=64000
[DISPLAY] ESP32-8048S070 init OK
[DISPLAY] P&P READY FOR WORK
[BUILD] source_commit=ab0417261e1009ee8d5f291605a492856410316e environment=stage8bp1s3sd
[P8-P1] SD owner mount OK
[WIFI] P&P AP STARTED ssid=Plonk-and-Play ip=192.168.4.1 clients=0
[DEV] Browser HTTP server STARTED error=0
[WIFI] STA_EVENT event=GOT_IP ip=192.168.68.102
```

SD, AP, remembered STA, mDNS and HTTP startup completed without display-init
failure, panic or reset. The READY screen remained completely steady and clean
for the requested approximately 30-second observation.

The normal production build does not emit largest/free heap snapshots; no
post-startup heap figures were available in this capture. The configured
runtime allocation is two 32,000-byte RGB565 internal DMA bounce buffers
(64,000 bytes total) plus the existing single PSRAM framebuffer.

## Status

**PASS — normal production RGB bounce-buffer configuration physically validated.**

