# SD-2 production Check storage physical checkpoint — bd2edb8

Date: 2026-10-29
Implementation: `bd2edb80fb1ba00068c691f2c04e8ea3b325453e`
Environment: `stage8bp1s3sd`
Artifact: `_approved-artifacts/bd2edb80fb1ba00068c691f2c04e8ea3b325453e/stage8bp1s3sd/firmware.bin`
Artifact SHA-256: `A9380D7CE19D69AAABA933DDE0C68AFCA12570B50002CE262764E0D230C43798`
Capture: `firmware/logs/device-monitor-bd2edb8-check-storage-baseline-2.log`
Capture SHA-256: `CBCDFC50FF0ECECF9F27C0141C436508BFD21E7D22BBB449D44446FD22B8B3E0`

## Setup

The exact preserved application binary was direct-flashed at `0x10000` after
its SHA-256 was verified. The SD card was inserted for boot. The ESP-ROM boot,
successful physical mount, bounded History exclusions for corrupt sequences 9
and 8, Diagnostics startup, AP/STA/mDNS/HTTP startup, and controlling tablet
context were captured. Connal verified that the production **Check storage**
control was present under Settings -> Diagnostics -> System Health and did not
press it during the baseline.

Connal then physically removed the SD card. Idle removal caused no poll,
remount, recovery, panic, reset, race mutation, or authority mutation. With the
dedicated COM7 capture already running, Connal pressed **Check storage** exactly
once.

## Observed check boundary

The production request was authorised in lifecycle `READY` and performed one
read-only filesystem check:

```text
[P8-P1] STORAGE_CHECK request=RECEIVED client=16757255406762592091 lifecycle=0
[P8-P1] STORAGE_CHECK attempt=1 path=/pp-p1 mode=READ_ONLY mount=0 recovery=0
[P8-P1] STORAGE_CHECK result=PHYSICAL_FAILURE health=UNAVAILABLE durable=0
[P8-P1] STORAGE_CHECK request=ACCEPTED result=PHYSICAL_FAILURE physical=UNAVAILABLE durable=0 history=UNAVAILABLE remount=0 recovery=0
[DEV] HTTP SLOW route=/request/check-storage duration_ms=2191 result=0 active=0 heap=195240 min_heap=184536
```

This proves the requested check, exactly one filesystem attempt, explicit
physical failure, owner transition to unavailable/non-durable, logical History
transition to unavailable, and a bounded HTTP completion in 2191 ms.

## First complete failure boundary

Immediately after the completed Check storage request, without reinsertion or
an authorised Retry storage request, the capture recorded:

```text
[P8-P1] SD owner mount begin cs=10 mosi=11 sck=12 miso=13
[P8-P1] SD owner mount OK
```

**FAIL — the controller autonomously entered the owner mount path after the
check reported physical failure.** This violates PSF.39 / PS15.94, which
requires Check storage to perform no mount/remount/recovery and leaves Retry
storage as the only explicit recovery action. The `mount OK` result is recorded
as observed; it does not change the decisive failure boundary, which is the
unauthorised mount attempt itself.

The capture was frozen at this first complete failure. No second Check storage
request, Retry storage request, SD reinsertion, reset, race, or Browser action
was performed. No panic, watchdog, reset loop, or retry storm was observed
before capture stopped.
