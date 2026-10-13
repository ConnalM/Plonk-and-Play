# Phase 8A Taster Wokwi surface

This isolated Wokwi project is derived from Gina's existing public Taster
prototype (`477221359503233025`). It deliberately keeps the classic
`firmware/diagram.json` ESP32 regression project unchanged.

The project models the reusable Phase 8A development boundary only:

- ESP32-S3 DevKitC-1;
- the prototype's 800x480 presentation chip;
- the prototype's KY-040 encoder on GPIO 5/6/7;
- the prototype's display UART on GPIO 17.

It does not claim to model the physical ESP32-8048S070 display, touch, SD,
PSRAM or detector wiring. The display chip is presentation-only. Race setup,
start, lifecycle, timing, detector input, results and records remain owned by
the P&P core through the Taster authority adapter.

The copied chip source is retained as a traceable reference asset from the
existing prototype. It is not a second race implementation.

Build from `firmware` with `build-taster-wokwi.ps1`; the resulting
`.pio/build/stage8atasterwokwi/firmware.bin` is the image referenced by this
project's `wokwi.toml`.
