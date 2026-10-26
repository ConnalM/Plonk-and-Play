# Framework-migration History decode analysis

Status: read-only investigation after the approved `7ffeed9` physical diagnostic.

## Evidence boundary

The running image reported source commit `7ffeed91b4869f2e7d00a400ad7ba68b0f0545ca`, environment `stage8bp1s3sd`, and ELF prefix `97de621ae`. The boot SD mount was healthy. A single non-destructive `GET /history` selected a payload and produced:

```text
[P8-P1] history_payload_decode length=338 checksum_ok=1 magic=0x50534631 wire_version=3 declared=338 format=3 expected_format=3 sealed=1
[DEV] HTTP ERROR route=/history duration_ms=39 result=260 active=0 heap=196836 min_heap=191808
```

There was no `history_payload_read` fault. The diagnostic therefore establishes that the selected file opened, its size matched the expected metadata size, the complete file was read, and its checksum and PSF1 envelope were valid. `result=260` is the explicit `ESP_ERR_INVALID_SIZE` result returned when the later codec decode fails. No write, repair, migration or deletion was performed.

The raw 338-byte payload is not present in the retained local capture or export artifacts, so it could not be decoded offline without another non-destructive byte capture.

## Wire-schema comparison

`VersionedResultCodec::encode()` and `decode()` use explicit little-endian field widths; they do not serialise C++ structs or depend on native alignment. For wire version 3, both sides use this order:

* PSF1 magic, wire version, reserved word, declared total length;
* result format version, flags, mode, finish behaviour, entry count, duration, reserved word, lap target;
* five 64-bit timing values, fastest-entry id, reserved word;
* per entry: seven 32-bit values, lane byte, flags byte, 32-bit record count, completion and best-lap 64-bit values;
* per record: lap number, start/finish/lap/racing-finish times (all 64-bit), validity byte and three bytes of padding;
* FNV-style 32-bit checksum.

The same codec implementation is present in the accepted pre-migration source (`ec899f9`) and the migration baseline (`a9636c4`); there is no migration diff in `persistent_storage.h`, `race_engine.h` or `core.h`. The current result and enum widths are unchanged and fixed-width on-wire values are used.

The current encoder's streamed-detail change (commits `b3d357d`/`588f508`) changed where record count/data are obtained, but did not change the wire layout or bump wire version 3. This is a format-governance risk for future compatibility, not proof of the present failure.

## Remaining decode gates

After the already-proven checksum and envelope fields, `VersionedResultCodec::decode()` can still return false at these gates:

1. Header reader extraction, outer version and declared-length equality (the diagnostic makes these values valid for this payload).
2. Result header extraction and `entryCount <= MaxEntries`.
3. Result format compatibility (`resultVersion == 3`, or the supported wire-v2/result-v2 legacy pair).
4. Mode range (`LapRace..Endurance`) and finish-behaviour range (`<= CompleteCurrentLap`).
5. Fixed result timing/header field extraction.
6. Per-entry fixed-field extraction.
7. Wire-v3 per-entry `recordCount` extraction.
8. Per-entry completion-time and best-lap extraction.
9. `LapRecordBuffer::prepare(recordCount, PP_REHYDRATED_DETAIL_BUDGET_BYTES)` allocation/budget check.
10. Per-record field extraction (including the explicit padding bytes).
11. Per-record `detail.append()` allocation/budget check.
12. Reconstructed record count equality.
13. Final reader position exactly equals `length - 4`.

The diagnostic did not yet print mode, behaviour, entry count, record counts or a failure stage, so the exact inner gate is not proven. A 338-byte valid v3 payload has the arithmetic shape `78 + 50*entryCount + 40*recordCountTotal`; the only normal two-entry/four-record shape is `entryCount=2`, total records `4`. That makes an ordinary count/budget failure less likely, but does not prove the per-entry distribution or values.

## Assessment

The first proven failing boundary is **inside `VersionedResultCodec::decode()` after envelope validation**. Source comparison does not show a framework migration change to the codec, filesystem read semantics or persisted format. The evidence therefore does not establish a direct ESP-IDF/Arduino migration regression.

The leading possibilities are a later payload semantic/range/record parse condition, or a latent compatibility problem in a stored version-3 payload. The exact gate remains unknown because the payload bytes and inner fields were not captured. There is no basis for changing the format, repairing data, or changing History recovery yet.

The smallest next diagnostic, if Archie authorises it, is fault-only decode-stage evidence (or a non-destructive raw-byte capture) that records mode, behaviour, entry count, each record count, buffer preparation/append result, reader position and the first failing gate. No production correction or further physical test is justified before that distinction.

