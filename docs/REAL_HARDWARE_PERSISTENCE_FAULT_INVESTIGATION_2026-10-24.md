# Real hardware persistence fault investigation — 2026-10-24

## Finding

The completed race at `firmware/logs/device-monitor-261024-161120.log` reached
normal sealing and emitted the expected crossing/finish activity, but it did
not emit `HistoryStored`.  The diagnostic export
`firmware/logs/P-and-P-Diagnostics-261024-162155-392.txt` contains a
`StorageHealthChange` record with `probe=0` in the old firmware, at the end of
the race persistence window.  This is the generic `StorageFault` path; it did
not identify which persistence boundary failed.

The exact race is absent from `/history`; an earlier committed record loads
with valid lap detail and non-zero finish durations.  Therefore the result
was not successfully committed to History.  The raw `/results` response was
the sealed in-memory fallback, which explains zero per-entry finish durations
after the Browser readiness fault path was allowed to present it.

The existing evidence cannot distinguish whether the terminal failure was:

1. streamed-detail loss/finalisation/incomplete detail; or
2. `VersionedHistoryStore::appendDetailed()` / backend History commit.

Both paths call the same `RaceEngineModule::latchPersistenceFault()` and emit
the same `StorageFault` event.  `SdActiveDetailStore::finalize()` and
`VersionedHistoryStore::appendDetailed()` expose only `bool`, so no operation
or backend reason survives into Serial or the diagnostic artifact.

## Narrow instrumentation

The implementation now carries a named, credential-free reason code in the
existing `Message::probe` field for the first published `StorageFault`, and
prints the same code/name in the existing Serial line.  The codes distinguish:

- record observation;
- streamed detail loss;
- streamed detail finalisation;
- streamed detail incomplete;
- result detail incomplete;
- detailed History append/commit;
- non-detailed History append/commit.

This is observation-only.  It does not alter persistence ordering, retries,
storage ownership, failure isolation, result sealing, Browser readiness, or
Race Engine timing.  The first terminal fault remains the one reported; no
historical evidence is rewritten.

The next occurrence will identify the failing top-level boundary directly in
the Serial stream and in the diagnostic `StorageHealthChange` payload.  If the
reason is `history_append_detailed`, a later, separately scoped backend reason
would be needed only if the append operation itself still requires finer
diagnosis.

The previously observed `STORAGE_FAULT` is consistent with this same generic
post-seal persistence-failure path, but the old evidence cannot prove whether
detail finalisation or History commit was first to fail.

## Current limits

No behavioural persistence correction is included here.  No physical race or
SD modification is requested until Archie reviews this instrumentation and a
single next occurrence supplies the missing boundary evidence.
