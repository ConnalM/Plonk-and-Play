# History payload decode result — `ae87916`

Status: read-only physical diagnostic; no production correction applied.

## Capture and identity

Capture: `firmware/logs/device-monitor-history-dump-com7-261026-5.log`

The running controller reported:

```text
[BUILD] source_commit=ae87916ecfb33e8a17c44877d5f38aa0e017410a environment=stage8bp1s3sd app_elf_sha256=9d68f34d8
```

The single non-destructive request was `GET http://192.168.68.102/history`. It ended prematurely as expected after the failing payload was read and decode rejected it.

The diagnostic identified History index `0`, sequence `9`, and emitted 22 complete hex lines. The exact reconstructed bytes are preserved in [`history-payload-sequence-9-ae87916.hex`](evidence/history-payload-sequence-9-ae87916.hex).

## Byte and checksum verification

* Reconstructed payload length: **338 bytes**.
* First hex offset: `0x00000000`.
* Last hex offset: `0x00000150`; final line contains 2 bytes.
* Device-computed FNV checksum: `0x533263f0`.
* Stored checksum: `0x533263f0`.
* Offline recomputation: `0x533263f0`.
* SHA-256 of the reconstructed 338 bytes: `73D06BFD819901A30E2857B58320EB4B66A09CCECD61CD30FBCC909AE26F83F6`.

The reconstructed bytes therefore match the device dump losslessly and pass the existing checksum rule.

## Offline v3 decode

The payload decodes through the envelope and all fixed-width fields up to the first semantic validation gate:

```text
magic=0x50534631
wire_version=3
reserved=0
declared_length=338
result_format=3
flags=0x03 (sealed=true, valid=true)
mode=1 (LAP_RACE)
finish_behaviour=2 (CompleteFullRaceDistance)
entry_count=2
duration_minutes=1
lap_target=2
winning_time=3560118498
finish_time=3567064126
expiry_time=0
duration_us=60000000
fastest_lap=3242516
fastest_entry_id=7
```

The two entries and four records also parse completely, and the reader ends at position `334`, exactly `length - 4`:

* Entry 0: race id 7, mug 91, lane 1, 2 classified laps, rank 1, flags `0x06`, record count 2, completion `3560118498`, best `3242516`.
* Entry 1: race id 8, mug 92, lane 2, 2 classified laps, rank 2, flags `0x06`, record count 2, completion `3567064126`, best `8853963`.
* All four records have valid fixed-width fields and zero padding bytes.

## Exact first failing gate

`VersionedResultCodec::decode()` contains:

```cpp
if (mode < static_cast<uint8_t>(SessionMode::LapRace) ||
    mode > static_cast<uint8_t>(SessionMode::Endurance) ||
    behaviour > static_cast<uint8_t>(LapFinishBehaviour::CompleteCurrentLap))
  return false;
```

The payload has a valid mode (`1`) but behaviour `2`. The current enum defines `CompleteFullRaceDistance` as `2`, while the decoder upper bound is `CompleteCurrentLap` (`1`). Therefore this exact valid v3 payload is rejected at the behaviour-range check before entry parsing is executed.

## Classification

This is a proven encoder/decoder validation defect, not an SD read failure and not a framework/compiler representation change. The encoder writes the existing enum value `2`; the decoder incorrectly excludes it. The payload is not malformed: it is sealed, checksummed, structurally complete and fully parseable offline.

The minimal production correction is to reconcile the decoder’s accepted behaviour range with the existing `LapFinishBehaviour` contract, while preserving wire version 3 and existing data. That correction is **not implemented in this evidence commit**.

Separately, the format-governance issue remains recorded: any future persisted-wire semantic/layout incompatibility requires an explicit compatibility assessment and, where appropriate, a wire-version bump/migration strategy.

