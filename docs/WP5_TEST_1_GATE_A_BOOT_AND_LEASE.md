# WP5 Test 1 — Gate A boot and retained controller lease

**Status:** Test 1 PASS; Test 2 prepared  
**Source commit:** `bd46a1c79274185e497806d35f69d988c023ad80`  
**Branch:** `browser-product-ux-stage15`

## Candidate

The Gate A candidate was rebuilt from the accepted source with the installed
PlatformIO Espressif32 6.9.0 toolchain and merged for the ESP32-S3 N16R8 board:

`firmware/.pio/build/stage8bp1s3sd/firmware.merged.bin`

- Size: 1,099,472 bytes
- SHA-256: `C6FD7218BEA1137C747FC2E0DB4CF37820520C9EF2C374D364E095AFD9395A62`
- Bootloader: `0x0000` (image header `E9`)
- Partition table: `0x8000` (ESP32 partition magic `AA`)
- `boot_app0.bin`: `0xE000` (image header `01`)
- Application: `0x10000` (image header `E9`)

The build reports 84,464 bytes of the 327,680-byte RAM budget and 1,033,569
bytes of the 6,553,600-byte application partition.

## Test purpose

This is the first bounded WP5 run for the Gate A candidate. It checks that the
candidate boots into the real P&P appliance and that the accepted retained
Browser controller identity is reconstructed after one controller restart.
It does not begin the wider SD, race-policy, multi-client or qualification
campaign.

## Human action

Flash the merged image above, connect the tablet to the P&P AP, take Race
Director control if the Browser asks for it, leave the tablet on the Network
page, then power-cycle the controller once and wait for the Browser to recover.

## Evidence required

Archie needs the single outcome of that run:

- the candidate boots and the P&P AP/Browser return;
- the same tablet identity regains Race Director control after recovery;
- the Network page is restored with authoritative status;
- a second Browser identity, if present, remains a spectator;
- no panic, unexpected reset, authority transfer or loss of the AP is seen.

Serial and Browser evidence should be retained automatically in the existing
`firmware/logs/device-monitor-*.log` and Browser/diagnostic paths. A failure
is recorded against the affected acceptance requirement before any further
WP5 action is chosen.

## Test 1 result — PASS

The human completed the controlled single-power-cycle run. The tablet
reconnected, the Browser recovered, the same tablet retained Race Director
authority, and the Network view was restored. No crash, reset loop or abnormal
behaviour was observed. This closes the first retained-authority/recovery
observation for the candidate; broader WP5 multi-client and race tests remain
separate.

## Test 2 — fresh two-lane Results/History path

The next bounded run uses the existing protected P1 simulated-detector fixture
to create one short two-lane Lap Race. It is a physical P&P/SD/Browser test;
the fixture still enters through Input Module → Message Bus → Race Engine.

### Test 2 result — FAIL (Results semantics and refresh)

The first physical run exposed two Results defects. Immediately after finish,
the winner showed target progress but TIME and BEST were `—`; opening
Details/History and returning to Results caused them to appear. A stopped
competitor that had completed one lap showed its progress and BEST, but TIME
was `—` even though Details and the Developer view contained the authoritative
3.40-second lap.

These are implementation defects, not expected presentation: Results must
refresh from the sealed authoritative result when the committed History
sequence becomes available, and TIME must show the authoritative elapsed
duration through a stopped racer's completed progress. An entry with no
completed lap alone remains `—`.

### Retest after the correction

Run one short two-lane Lap Race with the existing fixture and open Results
immediately after finish, before visiting Details or History. Verify that the
winner immediately shows its target progress, own TIME and BEST; verify that a
racer stopped after one completed lap shows `1 / target`, its lap elapsed TIME
and BEST (the physical reference was 3.40 s); verify GAP remains authoritative.
Then open Details/History and return to Results and confirm the same values,
with no HTTP error, panic, unexpected reboot or false remount.
