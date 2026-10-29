# SD-2 Check-storage post-failure mount analysis

Date: 2026-10-29  
Source analysed: `bd2edb80fb1ba00068c691f2c04e8ea3b325453e`  
Physical evidence: `369969ed5ad7320b1c44320def5d33750d306b35`  
Artifact exercised: `bd2edb8` / `stage8bp1s3sd`

## Finding

The Check storage request itself obeyed its contract. It performed one
read-only `/pp-p1` access, reported a physical failure, and
`PhysicalSdOwner::markIoFailureLocked()` changed the owner to unavailable and
non-durable. The subsequent `SD owner mount begin` was an autonomous call from
the pre-existing controller-loop recovery service. It was not caused by a
second Browser request, Diagnostics re-read, Retry storage request, or SD
reinsertion.

This is a production recovery-gate defect. After a runtime I/O failure, the
loop still treats the owner as eligible for its one automatic recovery attempt.
That violates PSF.39 and PS15.94: Check storage must not mount/remount/retry,
and Retry storage must remain the only explicit recovery action.

## Reconstructed call path

The exact `bd2edb8` path is:

1. `BrowserInterface::checkStorageRoute()` (`firmware/include/pp/browser_interface.h:2455-2519`)
   calls `PhysicalSdOwner::checkStorage()`.
2. `checkStorage()` (`firmware/include/pp/physical_sd_owner.h:115-143`) opens
   `/pp-p1` under an owner lease. With the card removed, the open fails and
   `lease.reportIoFailure()` is called; the route emits
   `PHYSICAL_FAILURE`, `physical=UNAVAILABLE`, `durable=0`, and
   `remount=0 recovery=0`.
3. On the next controller loop, `loop()` calls
   `servicePhysicalSdRecovery()` (`firmware/src/main.cpp:793-800`).
4. `servicePhysicalSdRecovery()` (`firmware/src/main.cpp:174-215`) sees
   `READY`, `available()==false`, no pending explicit request,
   `recoveryFailureLatched==false`, and `physicalRecoveryAttempted==false`.
   Therefore line 186 makes `automaticRequest=true`; line 195 calls
   `physicalSdOwner.recover()`.
5. `PhysicalSdOwner::recover()` (`physical_sd_owner.h:92-99`) calls
   `mountLocked()`; `mountLocked()` (`physical_sd_owner.h:222-232`) logs
   `SD owner mount begin` and calls `SD.begin()`.
6. If that mount returns true, the same service continues into
   `sdPersistence.recover()`, `rawHistory.reconstruct()`,
   `history.reconstruct()`, `activeDetail.recover()`, and
   `controllerTraceStorage.recover()` (`main.cpp:200-214`). This is an
   unauthorised post-check reconstruction path as well as an unauthorised
   mount path.

The setup path at `main.cpp:654-680` is separate and legitimate: normal boot
calls `physicalSdOwner.begin()` and performs initial namespace reconstruction.
The only direct production `PhysicalSdOwner::recover()` call is the loop
service above. Consumer `recover()` methods (`sd_store_arduino.h:282` and the
Diagnostics backend) do not call `SD.begin()`; they only reconstruct their
subordinate namespace when the owner is already available.

## Why the latch did not protect this path

`physicalRecoveryAttempted` and `recoveryFailureLatched` are controller-local
state in `main.cpp:86-91`. `Lease::reportIoFailure()` only calls
`PhysicalSdOwner::markIoFailureLocked()` (`physical_sd_owner.h:66-71,
216-221`); it does not set either controller-local flag. The Check request
therefore leaves both flags false. Because the lifecycle remains `READY`, the
automatic branch is immediately eligible. The latch is set only after
`recover()` itself fails or post-mount reconstruction fails
(`main.cpp:195-214`), which is too late for this failure boundary.

The automatic branch was introduced with the Retry storage implementation
(`25aed35`) and predates the Check storage feature (`bd2edb8`). Physical I/O
propagation was added in `579694a`; Check storage then made the latent
combination deterministic and visible. Thus this is not a Browser-authority
or Diagnostics re-read defect, and the same autonomous path could have been
entered after an earlier consumer I/O failure.

## Meaning of the observed `mount OK` with the card absent

The installed framework is Arduino-ESP32 3.3.12 (package
`framework-arduinoespressif32`, version `4.30312.261024`). Its
`libraries/SD/src/SD.cpp` implementation is decisive:

```text
SDFS::begin(...) line 29
  if (_pdrv != 0xFF) return true;       // lines 30-32
  ... sdcard_init(...)                  // line 38
  ... sdcard_mount(...)                 // line 43
```

`SDFS::end()` is the operation that unmounts/uninitialises and resets
`_pdrv` (`SD.cpp:54-61`). `markIoFailureLocked()` does not call `SD.end()`;
it only clears the P&P owner flags (`mounted_`, `recoveryInProgress_`,
`durable_`, `health_`). Consequently the unauthorised `mountLocked()` calls
`spi_.begin()` and then `SD.begin()`, but `SDFS::begin()` returns true from its
already-set `_pdrv` cache without reaching `sdcard_init()`, `sdcard_mount()`
or `f_mount()`. The observed `SD owner mount OK` is therefore a stale-wrapper
success, not evidence that the absent medium was reinitialised or mounted.

This also explains why the owner-level log can say `mount OK` while the
physical medium remains absent. The capture was stopped at the first
unauthorised mount boundary, so no claim is made here about the subsequent
namespace results or final health.

## Requirement comparison

- `docs/ACCEPTANCE_TESTS_PERSISTENT_STORAGE_FOUNDATION.md:57-59` (PSF.37–39)
  requires lifecycle-gated owner recovery, explicit Retry storage, and
  Check storage with no mount/remount/retry.
- `docs/ACCEPTANCE_TESTS_STAGE_15_LOGGING_DIAGNOSTICS_FOUNDATION.md:65-66`
  (PS15.93–94) makes the same request-only/reconstruction and read-only
  boundaries.
- `docs/SYSTEM_ARCHITECTURE.md:843-896` assigns mount, filesystem access and
  recovery to the single PhysicalSdOwner and keeps reinsertion inert.
- `docs/PERSISTENT_STORAGE_FOUNDATION_DESIGN.md:73-128` separates physical
  availability from logical health and leaves Retry storage as the explicit
  recovery action.

The observed post-check call violates those contracts even though the Check
route's own `remount=0 recovery=0` evidence was correct.

## Narrowest correction boundary (not implemented here)

1. Preserve setup-time `physicalSdOwner.begin()` and its normal boot
   reconstruction.
2. Make a runtime physical I/O failure set an owner-level
   `explicit-recovery-required`/failed-recovery latch that lifecycle changes
   cannot clear.
3. Change `servicePhysicalSdRecovery()` so that, after startup has settled,
   `PhysicalSdOwner::recover()` and all subordinate reconstruction are entered
   only after one accepted, consumed Retry storage request. A failed attempt
   consumes that request and remains unavailable until a new request.
4. Keep the existing lifecycle gate (`READY` or settled `FINISHED`) and
   one-shot pending-request semantics. No polling, card-presence loop,
   consumer mount, or new recovery state machine is needed.
5. Before a legitimate explicit retry, the owner must invalidate the Arduino
   SD wrapper's cached mount (owner-controlled `SD.end()`/equivalent teardown)
   so a retry cannot be reported successful solely because `_pdrv` remains set.
   This teardown must remain inside `PhysicalSdOwner`; consumers retain no
   mount authority.

The exact implementation choice (owner latch versus a narrowly scoped
controller startup/steady-state gate) requires Archie review before code
changes. The invariant is fixed: after any runtime physical I/O failure, no
autonomous mount/reconstruction may occur; only an authorised Retry storage
request may permit one owner attempt. Normal boot mounting remains unchanged.

## Evidence and next proof

The physical record shows the decisive order:

```text
[P8-P1] STORAGE_CHECK result=PHYSICAL_FAILURE health=UNAVAILABLE durable=0
[P8-P1] STORAGE_CHECK request=ACCEPTED ... remount=0 recovery=0
[DEV] HTTP SLOW route=/request/check-storage duration_ms=2191 result=0
[P8-P1] SD owner mount begin ...
[P8-P1] SD owner mount OK
```

Before any physical retest, software proof must cover: Check failure leaves
the owner latched unavailable; no loop iteration mounts or reconstructs; the
Arduino wrapper is torn down before a permitted retry; one accepted Retry
storage request causes at most one real mount/reconstruction attempt; failed
retry requires a new request; reinsertion remains inert; and normal boot
mount/reconstruction still succeeds. No code, build, flash, SD, Browser or
controller state was changed for this report.
