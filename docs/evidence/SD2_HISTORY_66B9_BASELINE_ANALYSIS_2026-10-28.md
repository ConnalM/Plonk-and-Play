# SD-2 mounted History baseline analysis — `66b9dc5`

Date: 2026-10-28  
Firmware: `66b9dc5d3520f429ab40255b26637117d5b93432`  
Environment: `stage8bp1s3sd`  
Capture: `firmware/logs/device-monitor-66b9dc5-mounted-20261028-214329.log`

## Observation

With the SD card inserted and the owner mounted/READY, the tablet opened the
normal History view. The Browser displayed **History is unavailable** and the
controller recorded:

```text
[DEV] HTTP ERROR route=/history duration_ms=37 result=260 ...
```

No SD owner transition, physical mount/recovery, panic, reset or race/authority
change followed. No further storage request was issued.

## Exact route boundary

`BrowserInterface::historyRoute()` allocates the result workspace, sends the
opening JSON chunk, then enumerates `history_->count()`. For each indexed entry
it calls `VersionedHistoryStore::loadNewest()`. Any false return, wrong decoded
length, unsealed result or wrong result format is collapsed to
`ESP_ERR_INVALID_SIZE`, which is result `260` on this build. The empty/lazy-list
path would have returned a successful empty JSON document, not result 260.

`VersionedHistoryStore::loadNewest()` delegates to `SlotHistoryStore::loadNewest()`
and then applies `VersionedResultCodec::decode()` and the sealed/format checks.
The route has no separate error for metadata, payload-size, checksum or semantic
decode failure.

## Startup reconstruction

`firmware/src/main.cpp` calls `rawHistory.reconstruct()` before committing
durable owner health. The log text still says “namespace reconstruction lazy”,
but that wording is stale: the call is present at the startup recovery boundary.
`SlotHistoryStore::ensure()` validates only the bounded metadata copies; it does
not open/decode every referenced payload. It returns true even when no valid
metadata copy is found, leaving an empty in-memory index. In this observation,
result 260 proves that the reconstructed index was non-empty enough for
`/history` to enter the payload loop.

## Why no physical I/O failure was reported

`Esp32SpiSdStore::readInternal()` has these relevant cases:

1. owner/store precondition or lease failure: false before `open()`;
2. required `open()` failure: owner failure is reported;
3. `File::size() != expected`: the file is closed, the destination is zeroed,
   and the function returns **true without calling `File::read()`**;
4. short `File::read()`: owner failure is reported; a complete read returns
   true.

A file-size mismatch therefore produces exactly this symptom: no physical read
failure and no owner degradation, followed by codec rejection and HTTP result
260. A complete read of malformed/unsupported bytes or a metadata/lease gate can
produce the same route result, but the current capture cannot distinguish those
sub-gates because this production path has no fault-boundary reason trace.

The strongest existing same-card evidence is the accepted SD-1 capture:

```text
history_payload_read slot=7 expected=45 open=1 file_size=41 bytes_read=0 result=size
history_payload_read slot=209 expected=45 open=0 file_size=0 bytes_read=0 result=open
```

That is the same silent size-mismatch behaviour. It is not an SD physical-open
failure and does not notify `PhysicalSdOwner`.

## 66b9 regression check

The diff from the approved `5d55fe6` implementation to `66b9dc5` changes the
Browser-generated Retry-storage control and the P1 diagnostic event observer;
it does not change `history_store.h`, `persistent_storage.h`,
`sd_store_arduino.h`, or the History route. The `/history` failure is therefore
not introduced by the Browser-only correction. The same class of mismatch is
already present in earlier 230f1ba SD-1 evidence.

The preserved sequence-9 payload analysis proves that a 338-byte v3 payload
with `CompleteFullRaceDistance` is valid after the decoder correction. It does
not prove that the current physical metadata entries point to that payload or
that sequences 9/10/11 were successfully read in this run.

## Classification and correction boundary

Classification: **pre-existing History storage/index/payload compatibility or
integrity defect; not a 66b9 Browser regression and not a proven physical SD
failure**. The narrowest proven boundary is after metadata enumeration and at
payload load/validation. The exact payload sub-gate remains unobservable in the
current production trace.

The smallest correction to review is:

- stop treating a file-size mismatch as a successful read with zero-filled
  bytes;
- return an explicit payload-integrity/format failure reason;
- make History reconstruction/route report or exclude corrupt/unsupported
  entries according to PSF.18/PSF.19 rather than silently presenting an
  unavailable list;
- keep physical owner health mounted when the medium is accessible, while
  reserving owner degradation for genuine physical I/O failures;
- add focused diagnostics/tests for metadata reconstruction, size mismatch,
  checksum/decode failure and valid v3 payloads.

No production correction was implemented in this analysis.
