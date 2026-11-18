# WP7 source-built ESP-IDF/Arduino double-buffer target

This is an opt-in build-only target for the ESP32-8048S070. It keeps the
existing PlatformIO/Arduino environment and the verified single-framebuffer
recovery image untouched. The target compiles the Arduino-ESP32 component and
the ESP-IDF RGB driver from source with `CONFIG_LCD_RGB_RESTART_IN_VSYNC=n`.

The Arduino component is supplied as a junction at `components/arduino` to the
installed Arduino-ESP32 package. The build also needs `PP_GFX_ROOT` and
`PP_TAMC_ROOT` pointing at the already-installed Arduino_GFX and TAMC_GT911
libraries. Build output belongs in this directory's `build/` and is not a
production artifact. Flashing is intentionally not part of this target.
