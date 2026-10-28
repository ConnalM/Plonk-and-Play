# SD-2 History integrity contract review — `66b9dc5`

Date: 2026-10-28  
Related analysis: `2d5a9fa`  
Current physical capture: `firmware/logs/device-monitor-66b9dc5-mounted-20261028-214329.log`

## Governing requirements

The authoritative persistence matrix requires:

- **PSF.18**: corrupt payloads and indexes are rejected, reported and excluded
  from reconstructed History;
- **PSF.19**: unsupported format versions follow the migration/reset boundary
  and are never decoded as another schema;
- **PSF.23**: restart/reconnect reconstructs committed History, Results and
  Details through the production Browser/API boundary;
- **WP2E.3**: storage health distinguishes healthy, low, insufficient,
  unavailable and logical corrupt/unsupported outcomes without changing race
  authority;
- **WP2E.4**: supported legacy data is read directly; corrupt/unsupported data
  is rejected safely and is not silently reinterpreted;
- `PERSISTENT_STORAGE_FOUNDATION_DESIGN.md` §SD failure cases: invalid data is
  not presented as official History, faults are visible through persistence
  status/diagnostics, and valid older committed records remain available where
  possible;
- the housekeeping policy removes only narrowly identified uncommitted
  temporary files. Committed payloads and metadata are not deleted as repair.

## Required behaviour for one bad retained payload

One corrupt or unsupported indexed payload must **not** make the entire
`/history` response unavailable when other committed entries can be read.
The bad entry must be rejected/excluded (or represented as an explicitly
quarantined entry), while valid retained entries continue to enumerate in
newest-first order with their original sequence identities.

The logical History/storage boundary must expose a bounded diagnostic reason,
including the sequence/index identity where known, expected versus observed
payload size where applicable, and a reason such as corrupt payload,
unsupported version or checksum/decode failure. The production API may return
valid entries plus a bounded fault summary; it must not fabricate the bad
record as valid.

Physical and logical health are separate. `PhysicalSdOwner` may remain
physically mounted/healthy when SD/FAT access succeeds. The History logical
capability must nevertheless be `Corrupt`/`Unsupported` (and surfaced through
the product's degraded/fault storage presentation) rather than claiming fully
healthy durable History. This does not change Race Engine authority or race
operation.

During startup or owner-controlled recovery, valid metadata must be inspected
without deleting committed evidence. A bad referenced payload is reported and
excluded; valid siblings remain available. Durable **fully healthy** History
must not be advertised until the required reconstruction contract is satisfied.
If the namespace is only partially usable, the physical owner can remain
mounted while the subordinate History state remains logically degraded/corrupt.

Automatic repair, rewrite, migration or deletion of the original committed
payload/index is not permitted for this case. Only explicitly identified
uncommitted `.tmp`/active-detail temporary objects may be reclaimed by the
existing mount housekeeping. Any future migration requires an explicit format
and evidence policy; this acceptance must preserve the original bytes.

## `readInternal()` contract

`Esp32SpiSdStore::readInternal()` must not treat
`File::size() != expected` as a successful read of zero-filled bytes. It must
return an explicit integrity/format failure with the expected and observed
sizes. That logical integrity failure must not be promoted to
`PhysicalSdOwner` unavailable unless the physical operation itself fails
(for example open failure or a short read). A complete read followed by
checksum/codec rejection is likewise a logical corrupt/unsupported result.

## Existing physical evidence

The current `66b9dc5` capture proves only:

```text
/history -> HTTP result=260 in 37 ms
owner remains mounted/READY
```

It contains no per-entry History diagnostic, so it cannot map the failing
record to sequence 9, 10 or 11. No host filesystem access to the live SD card
is available without removing or mutating the controller medium, which is out
of scope.

The earlier accepted same-card SD-1 evidence recorded:

```text
history_payload_read slot=7 expected=45 open=1 file_size=41 bytes_read=0 result=size
history_payload_read slot=209 expected=45 open=0 file_size=0 bytes_read=0 result=open
```

This proves an existing slot-level size mismatch/open observation, but does not
prove which History sequence each slot represents. The preserved offline
sequence-9 payload is a valid 338-byte PSF v3 record after the decoder fix, but
that artifact does not establish that the current SD metadata points to it.

## Correction boundary

The next implementation review should therefore remain narrow:

1. make size mismatch a non-successful, explicitly classified logical payload
   result;
2. distinguish physical I/O failure from corrupt/unsupported payload status;
3. reconstruct/enumerate valid History entries while excluding bad entries;
4. surface per-entry/fault evidence through the existing persistence/diagnostic
   boundary; and
5. preserve all committed bytes and all Race/authority semantics.

No production code or SD contents were changed by this analysis.
