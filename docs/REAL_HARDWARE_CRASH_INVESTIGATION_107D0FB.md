# Real-hardware crash investigation: 107d0fb

**Investigation date:** 2026-10-21  
**Firmware:** `107d0fb17ae3a0f3d6aa6f6b3215202b28c7c241`  
**Serial evidence:** `firmware/logs/device-monitor-261021-153607.log`  
**Matching ELF:** `firmware/.pio/build/stage8bp1s3sd/firmware.elf`  
**ELF SHA-256:** `074B375350F05048FC5F52CCF414CF0D195E9E4703A4812EE84136A818115866`

This is an investigation-only record. No firmware or storage was changed.

## Crash evidence and decode

The complete capture shows one initial power-on boot and, at approximately
`system_us=4,085,490,000`, repeated diagnostic rolling cleanup messages followed
by:

```text
Guru Meditation Error: Core  1 panic'ed (LoadProhibited). Exception was unhandled.
EXCCAUSE: 0x0000001c
EXCVADDR: 0x00000028
Backtrace: 0x4200bcb3:0x3fcb2010 0x4200beaa:0x3fcb20c0 0x420463f2:0x3fcb2100 0x42045430:0x3fcb2130 0x42045b6a:0x3fcb21d0 0x42044914:0x3fcb21f0 0x420a2416:0x3fcb2210 0x42044a24:0x3fcb2230
ELF file SHA256: 074b375350f05048
```

The addresses decode against the matching ELF as follows:

| PC | Decoded function/source |
|---|---|
| `0x4200bcb3` | `pp::BrowserInterface::serializeResults(...)`, generated `browser_interface.h:857` |
| `0x4200beaa` | `pp::BrowserInterface::resultsRoute(...)`, generated `browser_interface.h:952` |
| `0x420463f2` | ESP-IDF `httpd_uri` |
| `0x42045430` | ESP-IDF `httpd_req_new` |
| `0x42045b6a` | ESP-IDF `httpd_sess_process` |
| `0x42044914` | ESP-IDF `httpd_process_session` |
| `0x420a2416` | ESP-IDF `httpd_sess_enum` |
| `0x42044a24` | ESP-IDF `httpd_thread` |

The failing application line is the presentation-only finish-duration loop:

```cpp
const auto& lap = entry.records[i];
if (!haveOrigin) { origin = lap.startTime; haveOrigin = true; }
```

`EXCVADDR=0x28` is consistent with dereferencing an invalid/null detail-record
base while reading the lap fields. The call path proves this was an HTTP
`/results` request, not an SD mount or diagnostic-export call. The request
never reached its normal completion trace because the HTTPD task panicked while
serializing the response.

## Root cause

`RaceEngineModule::seal()` deliberately leaves `ResultEntry::records` empty
when the production `SdActiveDetailStore` owns the lap records on SD. It still
sets `ResultEntry::recordCount` from the streamed detail count. The current
in-memory result therefore legitimately has a non-zero count but no RAM record
pointer.

`BrowserInterface::finishDurationUs()` assumes that every result entry with a
non-zero `recordCount` also has `entry.records` in RAM. It does not use the
existing `DetailSource`/SD detail boundary and has no null/availability guard.
When the Browser requests Results for that sealed result, the assumption is
false and the HTTPD task crashes. This is an unambiguous implementation defect
in the Results presentation boundary; the PhysicalSdOwner is not the cause.

The reboot immediately following the panic mounted the SD owner successfully
again. That is evidence against the crash being caused by the physical SD
mount, although the crash can still interrupt an active race or an in-flight
persistence operation and is therefore a serious product defect.

## `records.roll.tmp` evidence

The repeated messages are emitted by the diagnostic SD rolling path. The
diagnostic backend has a bounded capacity of 256 records. Once full,
`ProductionDiagnosticRecorder::tryAppend()` asks
`Esp32SpiSdDiagnosticStorage::evictOldest()` to roll the oldest record. Each
roll begins with this best-effort cleanup:

```cpp
owner_.remove("/pp-diagnostics/records.roll.tmp");
```

The path normally does not exist after the previous roll has renamed it into
`records.bin`, so Arduino VFS prints the `does not exists or is directory`
error. The return value is intentionally ignored before the temporary file is
created. The same sequence continues after reboot, which is consistent with
normal bounded rolling at capacity rather than a mount failure.

The removal error is therefore noisy cleanup/observability debt, not the crash
cause. The decoded stack contains no diagnostic SD rolling frames. There is no
evidence in this capture that the missing temporary file corrupted
`records.bin`; the post-panic reboot remounted the card and continued booting.
The cleanup should remain a separate narrow follow-up and must not be used to
mask or conflate the Results crash.

## Consequences and current classification

* **Crash:** confirmed, reproducible in the production Results HTTP path when
  streamed detail is present. Severity is high: a Browser Results request can
  reboot the controller and can interrupt an active session.
* **SD/diagnostic integrity:** no direct filesystem corruption is shown. The
  card remounted after the panic, but the interrupted operation means this run
  cannot be used as a clean race/storage acceptance run.
* **Diagnostic rolling:** bounded rolling is active and the repeated missing
  temp-file messages are a separate noisy cleanup issue. They are not proven to
  cause the panic.
* **Product decision:** none is required for the crash correction. The smallest
  correction is an implementation change to make Results obtain finish timing
  through the existing streamed-detail boundary (or safely report unavailable)
  instead of dereferencing `ResultEntry::records` when the records are not
  resident. That correction must be implemented and tested separately from the
  temp-file cleanup.
