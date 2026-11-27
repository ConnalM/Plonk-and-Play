# WP8 Core temporary touchscreen-board bench plan

## Status

Preparation and restoration plan only: no ESP configuration, build, image,
flash, COM/SD access or hardware action is authorised by this document. The
accepted Taster remains the recovery device.

## Purpose and boundary

The ESP32-8048S070 N16R8 touchscreen may be considered only as a temporary
Core bench host while the independent Core board is pending. Its screen and
touch are not Core UI. No Taster link/pairing, detector, SD or presentation
service may be activated.

Initial bench evidence is limited to boot/idle stability, P&P access point and
Browser authority, isolated Core NVS configuration and rejected START due to
unavailable required detector capability. It cannot qualify timing, detector
health, SD recovery, History/PB/Track Record persistence, pairing or two-board
operation.

The Core bench source starts only the P&P access point. It neither joins,
inspects, replaces nor erases remembered station credentials, and Browser
network-provisioning requests are rejected. That preserves the host's existing
network state; it is not a substitute for the restoration gate below.

## Mandatory board facts before a temporary image

Verify exact board revision, display, backlight, touch, boot/strap, USB/UART
and flash/partition facts from accepted records. Do not transfer proposed Core
GPIOs to this board or reuse its SD/display/touch wiring. The Core component
has no display or touch service; suitability is unproven until its reset-safe
panel/backlight behaviour is confirmed.

## Reversible restoration gate

Before any temporary flash, retain and verify outside the board:

1. current accepted application SHA-256 and matching recovery binary;
2. bootloader, partition table and application identities/hashes;
3. read-only record of NVS namespaces and SD/History contents;
4. tested restoration instructions for the accepted application-only image;
5. after-restoration checks: boot identity, READY display, Browser, settings,
   History, PBs and Track Record.

If this cannot be demonstrated without changing customer data, defer physical
testing until the independent Core board is available. The Core namespaces
`pp-core` and `pp-core-auth` do not replace preservation evidence.

## Separate approvals

Configure disposable Core build; compile/link a candidate; authorise a
temporary flash; then authorise bounded physical observation. Each is separate.
