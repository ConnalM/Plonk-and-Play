# SD-2 idle physical-removal checkpoint — 388a5f3

Date: 2026-10-28  
Firmware: `388a5f3e6fe535c9b78a3af21c47034ffebdf5d2`  
Capture: `device-monitor-388a5f3-history-integrity.log`  
Capture SHA-256: `CDB4B3AA44FE75F9F6E9E8C3854E16C1DA3A87E5AFD2B548D1A5FE825245BD9D`

Connal reported the SD card removed while the controller remained powered. The
first live observation after that report was timestamped `2026-10-28
23:00:33.681 UTC`; the capture was then observed through approximately
23:00:52 UTC.

## Passive-removal result

- Controller continued running normally.
- Browser/AP/HTTP and the existing authority context remained unchanged.
- No SD owner, History, or Diagnostics transition occurred from idle removal.
- No `SD.begin()`, remount, recovery, retry loop, panic, reset, watchdog, race,
  or authority mutation occurred.
- Repeated `BOOT_SELF_TEST=PASS` lines continued as startup/self-test
  observability; they are not live medium-presence polling.

No storage request or other controller action was issued during this checkpoint.
