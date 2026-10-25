# WP5 History commit stack-canary investigation — 2026-10-25

## Status

Investigation only. No firmware, persistence behaviour, Results behaviour or
networking behaviour was changed.

## Physical evidence

The continuous capture is
`firmware/logs/device-monitor-261025-183547.log`, from source commit
`5631a8912dd815a4d163df080a2ca0ce9cc82b45` / `stage8bp1s3sd` (ELF SHA prefix
`648074608bb392d4`).

The race reached active-detail completion successfully:

- entry 0 seal expected/observed: `104/104`;
- entry 1 seal expected/observed: `64/64`;
- `detail_finalize success` was emitted.

Immediately afterwards the capture shows:

```
open(): /sd/pp-p1/slot-208.bin does not exist, no permits for creation
remove(): /pp-p1/slot-11.bin.tmp does not exists or is directory
Guru Meditation ... Stack canary watchpoint triggered (loopTask)
```

The controller rebooted. After reboot the SD owner mounted successfully and
the STA/AP remained connected. No STA/IP loss caused this event.

## `slot-208.bin` classification

The message is not proof that creation of the race payload failed. In the
current code, `SlotHistoryStore::ensure()` probes both metadata copies:

- `MetaSlotA = 6`;
- `MetaSlotB = 208`.

The second copy is intentionally outside the 200 data slots and may be absent
on a card that has not yet published that metadata generation. The probe calls
`Esp32SpiSdStore::read(208, ...)`, which opens the path with `FILE_READ`.
ESP32 Arduino FS emits the generic `no permits for creation` wording when a
read-only open finds no file. It is therefore an expected missing-metadata-copy
diagnostic, not a failed `FILE_WRITE` creation attempt.

## Decoded call path

The matching ELF symbols decode the captured backtrace as:

```
loopTask
  -> loop
    -> RaceEngineModule::tick
      -> RaceEngineModule::persistResult
        -> VersionedHistoryStore::appendDetailed
          -> SlotHistoryStore::append
            -> SlotHistoryStore::publishMetadata
              -> Esp32SpiSdStore::write
                -> PhysicalSdOwner::remove(String)
                  -> fs::FS::remove / VFSImpl::remove
```

The panic occurs while the metadata commit is still in progress, before the
History append can publish `HistoryStored`. The physical filesystem was
mounted and active-detail sealing had already passed.

## Root cause

`SlotHistoryStore::append()` keeps a full `MetadataState candidate` automatic
object on the loop task stack. `publishMetadata()` then creates automatic
buffers for both encoded and verification metadata copies plus another
`MetadataState checked` object. The ESP32 loop task has a bounded stack. The
History metadata commit consequently leaves insufficient stack headroom for
the nested SD/VFS temporary-removal path; the ESP-IDF stack canary fires.

The source and call path prove that the crash is in the History metadata commit
boundary. The exact byte at which the stack canary is overwritten is not
reported by the panic, but the automatic metadata workspaces and the decoded
call path identify the bounded-stack violation. The `slot-208` read-probe and
the recurring diagnostics rolling cleanup messages are separate from that
stack exhaustion.

## Smallest production correction proposed

Move the bounded metadata commit workspace out of automatic function stack
storage into an explicitly owned, serialized workspace (heap/static ownership
with a clear lifetime and allocation-failure path). Keep the existing
`operationBusy_` guard, metadata slot scheme, transaction order, formats,
verification, retention and failure isolation unchanged. A workspace
allocation failure must return the existing persistence failure without
publishing an uncommitted History result.

The `records.roll.tmp` cleanup noise remains a separate observability issue.
