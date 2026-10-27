# HTTP/SSE idle `pp_sse_3` canary analysis

## Evidence

Capture: `firmware/logs/device-monitor-261027-140859.log` (the failed run recorded
in commit `61c89c6`). The run booted the image rebuilt from `16ae810`, then before
any client request printed:

```text
[SSE_STACK] generation=0 fd=-1 stage=idle hwm_words=1520 stack_words=2048 cleanup=none
[SSE_STACK] generation=0 fd=-1 stage=idle hGuru Meditation Error: ...
Debug exception reason: Stack canary watchpoint triggered (pp_sse_3)
```

The first boot rebooted. The second boot reached normal AP/STA/mDNS/HTTP startup and
printed four idle HWM lines (`1528`, `1520`, `1520`, `1520`) without another panic.
No SSE request or transport-pressure action occurred.

## Code boundary

`startHttpWorkers()` creates two page workers and four SSE workers. Each SSE task is
passed the address of its corresponding element in the persistent `sseStorage_[]`
member array; `owner` is assigned before `xTaskCreate()`. The task parameter is not a
stack address, and the Browser object is a global-lifetime object. Session-owned frame
buffers and the task handle are fields of that persistent record. No out-of-bounds write
or invalid task-parameter lifetime is visible in this startup path.

The idle task immediately calls `recordSseStack(session, "idle", ...)`. That function
samples `uxTaskGetStackHighWaterMark(nullptr)` and then calls `Serial.printf()` with the
session fields and cleanup text. The capture truncates during the second idle line,
while the panic is reported as `pp_sse_3`. Thus the narrowest proven boundary is the
idle worker's diagnostic call path, specifically the post-HWM diagnostic formatting/
output path (or memory already corrupted by that path), not an SSE send, browser request,
or race/persistence operation.

The reported HWM is sampled before the `Serial.printf()` call. `1520` remaining words
means approximately 6080 bytes of untouched stack at that sampling point; it is not a
peak measurement for the subsequent formatting/output call and therefore does not prove
that the entire idle path fits. A canary can also be tripped by a one-shot frame or an
adjacent overwrite after the sample, so the HWM alone cannot distinguish stack peak from
adjacent-memory corruption. Source inspection finds no independent task-lifetime or
session-array defect that explains the event. The `vTaskDelete(nullptr)` branch lacks an
explicit return, but it is not reached for these workers because `owner` is assigned
before creation.

This is evidence for a diagnostic-path/worker-stack boundary, not authorization to
increase the stack blindly. The next software correction/measurement must either move the
diagnostic formatting out of the worker or measure it without a large `printf` frame,
then re-measure the actual send and cleanup paths.

## 56ee14a versus 16ae810

`git diff --name-status 56ee14a0fba4b4f9549ee0dc4ee8cd43be4f11f1..16ae8107346c94eecf4d61fe96bf25dacf5a8e42`
contains only `docs/evidence/HTTP_SSE_ISOLATION_DIAGNOSTIC_PROOF_20261027.md`.
There is no executable/source/configuration difference between the two commits; the
runtime difference is the embedded build provenance (and normal build metadata), not the
SSE implementation.

## No-rebuild artifact procedure

The approved binary must be preserved immediately after its approved build, for example:

```text
_approved-artifacts/56ee14a0/stage8bp1s3sd/firmware.bin
_approved-artifacts/56ee14a0/stage8bp1s3sd/manifest.txt
```

`manifest.txt` records source commit, environment, byte size and SHA-256. Before a
physical run, hash that fixed file and compare it byte-for-byte with the manifest. If
either size or hash differs, stop. Do not run `pio run -t upload`, because that can
rebuild the current checkout.

For this candidate the required manifest is:

```text
source_commit=56ee14a0fba4b4f9549ee0dc4ee8cd43be4f11f1
environment=stage8bp1s3sd
size=1478656
sha256=C53F29910C8B51FEBA0ECE1615A9ADB2C17934F2AA8163A3EC27D0CCD8D2FDB5
```

With COM7 released, flash the preserved application directly at the PlatformIO app
offset, without invoking PlatformIO's build/upload target:

```powershell
C:\Users\conna\.platformio\penv\Scripts\python.exe -m esptool --chip esp32s3 --port COM7 --baud 460800 --verify write_flash 0x10000 "<preserved-artifact>\firmware.bin"
```

If a preserved, hash-verified merged image is supplied instead, flash that image directly
at `0x0` with the same esptool invocation; never rebuild it. After reset, capture boot
Serial and `/health`. Require both runtime source commit `56ee14a…` and environment
`stage8bp1s3sd` before any test action; any mismatch aborts the run.
