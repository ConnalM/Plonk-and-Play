# Framework migration smoke investigation: display and History

Date: 2026-10-26  
Candidate: `a9636c43ab89cca6ab4bb9e656c143b1e0e27e76` (`stage8bp1s3sd`)  
Smoke evidence: `16bb42f5079b62e34c8cdb40a14474b62d015265`  
Capture: `firmware/logs/device-monitor-migration-261026-142430.log`

This is a read-only investigation. No firmware, NVS or SD data was changed.

## Display

The durable smoke record says only that the visible display was “not as expected”; the follow-up operator reply was “it isn't”. That does not identify blank/backlight-only, partial text, geometry, colour, orientation or corruption. The exact visual symptom is therefore not established.

The P&P display adapter is unchanged between the accepted pre-migration source (`ec899f9`) and `a9636c4`: `firmware/include/pp/board_display_esp32s3.h` still constructs the same `Arduino_ESP32RGBPanel` and `Arduino_RGB_Display`, with the same 800x480 geometry, pins, 12 MHz PCLK, porch values, polarity, PSRAM framebuffer and `begin()`/rotation/READY sequence. The boot capture reports `[DISPLAY] ESP32-8048S070 init OK` and `[DISPLAY] P&P READY FOR WORK`.

The migration did change the executed graphics/framework path:

| Boundary | Pre-migration | Migration | Relevant difference |
|---|---|---|---|
| Platform/framework | Espressif32 6.9.0, Arduino-ESP32 2.0.17-era path | Espressif32 8.0.0, Arduino-ESP32 3.3.12, ESP-IDF 5.5.5 | New Arduino/IDF RGB-panel implementation is exercised |
| GFX library | Arduino_GFX 1.5.6 | Arduino_GFX 1.6.5 | Required Arduino 3 SPI API compatibility; RGB display adds P4 support, a skip-bus-begin sentinel, and revised rotation write paths. The current call uses the default `begin()` and rotation 0, so the sentinel and non-zero rotation paths are not exercised. |
| RGB panel config | Arduino-2 branch | Arduino-3/IDF-5 branch | Explicit `bits_per_pixel=16`, `num_fbs=1`, `bounce_buffer_size_px=0`, `double_fb=false`, `no_fb=false`, cache flag false; `LCD_CLK_SRC_DEFAULT` rather than the pre-3 `PLL160M` branch. `fb_in_psram=true` remains. |
| P&P parameters | Existing known-good values | Unchanged | No P&P timing/pin/PCLK edit caused this migration candidate. |

The changed IDF/Arduino RGB-panel allocation/configuration is a plausible migration compatibility boundary, especially because the framebuffer is allocated by `esp_lcd` in PSRAM, but the current evidence does not identify a visual failure mode or prove which changed default is responsible. No minimal compatibility source change is justified yet. A focused display observation with the symptom classified is required before selecting one.

## History

The first captured boot mounted the same physical card successfully (`card=15931539456`, filesystem `15926558720`, used `22282240`). `/records` returned the persisted lane PB/Track Record values, and no SD/FAT/SPI error surrounds the `/history` request. The pre-migration captures recorded successful `HistoryStored` sequences 6, 7, 8 and 9 on this card. This is evidence against assuming general SD corruption or loss.

The current `/history` response is incomplete: `{"entries":[`. The capture then reports:

`[DEV] HTTP ERROR route=/history duration_ms=36 result=260 active=0 ...`

`260` is the route's explicit `ESP_ERR_INVALID_SIZE` sentinel, not an ESP-IDF filesystem error. `historyRoute()` in `firmware/include/pp/browser_interface.h` sends the opening chunk, obtains `history_->count()`, then rejects the first entry if any of the following combined checks fails:

```cpp
!history_->loadNewest(...)
|| bytes != sizeof(*stored)
|| !stored->sealed
|| stored->formatVersion != RaceEngineModule::ResultFormatVersion
```

It also uses the same sentinel for an over-sized `snprintf`, but that formatting code is reached only after the payload has loaded and passed the combined guard. Because the opening chunk was sent and the result is 260 rather than the transport error 45062, this is not the observed `/normal`/HTTP send failure. It is a History load or validation failure after metadata enumeration.

The read chain is:

`historyRoute()` → `VersionedHistoryStore::loadNewest()` → `SlotHistoryStore::loadNewest()` → metadata-selected payload slot → `Esp32SpiSdStore::read()` → exact `File::size()`/read check → versioned decode → sealed/format validation.

The current source and capture do not distinguish these sub-cases:

- payload file cannot be opened or has a size different from the metadata entry;
- short payload read;
- versioned checksum/schema decode failure;
- decoded result is not sealed or has an unexpected format version.

The migration introduces a direct filesystem compatibility hypothesis. The old Arduino FS VFS opened read files after a path `stat()` and retained that `stat` result. The new Arduino 3 FS VFS opens first and obtains the read-file metadata with `fstat(fileno(_f), &_stat)`. `Esp32SpiSdStore::read()` relies on `File::size()` being correct before it reads, so this is a migration-sensitive boundary. It is not proven: metadata reconstruction must have succeeded sufficiently for `count()` to enumerate entries, while the failure may be limited to a payload file or to decode/validation.

No History data was deleted, rewritten, migrated or repaired. The smallest next diagnostic, if approved, is one fault-only read-boundary trace for the selected payload: slot, expected metadata size, `File::size()`, bytes read, and decode/sealed/version outcome. That would identify the first failing sub-operation without changing storage behaviour.

## Separate HTTP observation

The capture also contains `HTTP ERROR route=/normal ... result=45062`. This remains a separate transport observation and was not used to explain the display or History failure.

## Current classification

- Display: migration compatibility regression is possible but the visual symptom is not classified; no source fix selected.
- History: the first proven boundary is payload load/validation after metadata enumeration; exact sub-operation is unknown. The existing pre-migration successful commits and current healthy mount make a migration-sensitive FS read or format/size compatibility issue more likely than general card loss, but this remains a hypothesis until the read boundary is observed.
- Further physical action: do not run a race or alter the card. A precise display symptom is needed; History needs the focused read-boundary evidence before a production correction.
