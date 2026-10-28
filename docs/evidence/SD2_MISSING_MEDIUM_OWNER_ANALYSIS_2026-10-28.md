# SD-2 missing-medium owner/health analysis

Date: 2026-10-28  
Firmware: `230f1ba09465f3751aa9916624b78848f2457925`, `stage8bp1s3sd`  
Physical evidence: `firmware/logs/device-monitor-sd2-history-probe-route-230f1ba-20261028.log` (capture retained by evidence commit `323c27bf005623ba5094c4cfc673c60d1ec87b81`)

## Conclusion

This is a real physical-media loss, not an HTTP or Browser failure. With the SD card physically absent, one `/history` request reached the controller and failed at the first payload open. The failure was explicit and bounded (HTTP result 260 after about 2.17 s), with no remount/retry storm, panic, reset, race mutation or authority change. The defect is that the subordinate SD store reports the failed open only as a local Boolean; it does not notify `PhysicalSdOwner`, so the owner remains `mounted_/Mounted`, the logical durable-health state remains healthy, and the existing owner recovery gate never becomes eligible. `BOOT_SELF_TEST=PASS` is therefore a cached/startup readiness result, not proof of current medium accessibility.

This is an implementation/conformance defect in the storage health propagation boundary. It is not evidence of corruption or a need for consumer-side mounting.

## Frozen requirements

The governing documents require the following.

- `docs/MASTER_PROJECT_PLAN.md`, WP5 section: WP5 must close “SD reboot reconstruction, second-race retention, removal/remount, failure/recovery and interruption obligations” and the “diagnostics download/degraded-storage physical path”.
- `docs/ACCEPTANCE_TESTS_PERSISTENT_STORAGE_FOUNDATION.md`, PSF.27: “SD removal, remount and recovery in the permitted non-active lifecycle states preserve committed records and expose unavailable/pending state without changing authoritative race state; recovery is deferred during prohibited session states.”
- PSF.31: one physical SD owner performs the only FSPI/`SD.begin()` mount; History, active-detail and Diagnostics do not independently mount/remount.
- PSF.32: one successful mount is shared; physical mount failure produces the defined independent logical degraded/fault states; Diagnostics may use RAM-only/degraded operation.
- PSF.33: reboot or explicit owner-controlled recovery in an eligible non-active state mounts once, reconstructs History/Diagnostics, and preserves committed records and sequence/retention semantics.
- PSF.35: slow/failing SD access cannot alter racing, results or HTTP; failed History remains uncommitted and diagnostics follow bounded degraded/RAM-only rules.
- PSF.36: recovery is requested only through the physical owner; logical consumers cannot retry `SD.begin()` independently.
- PSF.37: recovery is suppressed during `STARTING`, `RACING`, `PAUSED`, `RESTARTING` and unfinished settlement; in `READY`/settled `FINISHED` exactly one owner-controlled mount occurs, subordinate namespaces reconstruct, then durable health is advertised.
- `docs/ACCEPTANCE_TESTS_PERSISTENT_STORAGE_FOUNDATION.md`, WP2E.3: capacity/health distinguishes healthy, low, insufficient and unavailable without changing authoritative racing.
- `docs/ACCEPTANCE_TESTS_STAGE_15_LOGGING_DIAGNOSTICS_FOUNDATION.md`, PS15.84: storage failure produces authoritative capability state and useful degraded behaviour without blocking or mutating Race Engine/product authority; PS15.88: Diagnostics uses the single physical owner and never independently initializes/mounts/remounts; PS15.90: physical SD failure is distinct from diagnostic logical health and uses bounded RAM-only/degraded behaviour where permitted; PS15.91–PS15.92: reboot/explicit owner recovery reconstructs through the shared owner and advertises durable health only after reconstruction.
- `docs/SYSTEM_ARCHITECTURE.md`, §13.0: one owner controls FSPI, mount, filesystem access and recovery; consumers do not call `SD.begin()` or opportunistically remount; unavailable/degraded physical health must propagate without affecting racing/HTTP; recovery is owner-controlled and lifecycle-gated.
- `docs/PERSISTENT_STORAGE_FOUNDATION_DESIGN.md`, “Physical SD ownership” and “SD failure cases”: the owner reports physical availability separately from logical health, no consumer may claim durable SD operation while the device is unavailable, and failed recovery leaves physical/logical health unavailable/degraded.

## Exact failure path

1. Boot had already mounted the card. `PhysicalSdOwner::mountLocked()` in `firmware/include/pp/physical_sd_owner.h:122-130` set `mounted_=true` and `health_=Health::Mounted`.
2. The card was then physically removed while powered. No idle path touched a payload, so no owner-visible I/O error occurred.
3. `/history` entered `BrowserInterface::historyRoute()` (`firmware/include/pp/browser_interface.h:1029-1049`). It enumerated `history_->count()` and called `VersionedHistoryStore::loadNewest()` for the newest retained record.
4. `VersionedHistoryStore::loadNewest()` (`firmware/include/pp/persistent_storage.h:112-128`) delegated to `SlotHistoryStore::loadNewest()` (`firmware/include/pp/history_store.h:84-93`), which delegated to `Esp32SpiSdStore::read()`.
5. `Esp32SpiSdStore::read()` (`firmware/include/pp/sd_store_arduino.h:89-104`) acquired the owner lease, called `owner_.open(path(slot), FILE_READ)`, and returned `false` when the open failed (`history_payload_read ... result=open`). It did not change `mounted_`, `lastHealth_`, or the owner's state.
6. The history route converted the false load/validation result into `ESP_ERR_INVALID_SIZE`, completed the HTTP trace, and returned the bounded HTTP failure. The accepted capture records HTTP result 260 after approximately 2172 ms.

The owner has no post-mount failure transition method. `available()` and `health()` only expose the cached fields (`physical_sd_owner.h:95-96`), and those fields are assigned only by `mountLocked()` (`physical_sd_owner.h:122-129`). The loop recovery service (`firmware/src/main.cpp:164-184`, called at line 754) attempts recovery only when `!physicalSdOwner.available()` in an eligible lifecycle. Because the failed open leaves `available()==true`, recovery is never requested.

## Why health stayed healthy

`Esp32SpiSdStore::health()` (`sd_store_arduino.h:53-59`) returns `Unavailable` only when its own `mounted_` flag is false; otherwise it derives health from capacity/free-space queries. A failed payload open does not clear that flag. `PhysicalSdOwner::health()` likewise remains `Mounted` because it has no I/O-failure input. The diagnostics SD backend is a separate logical consumer: its durable status is updated by its own append/scan operations, not by a History read failure (`firmware/include/pp/diagnostic_sd.h:154-160`). Therefore the `/history` read failure did not update the diagnostic storage status either.

The `/health` handler is currently a minimal network liveness response (`firmware/include/pp/browser_interface.h:2384-2387`) and does not report storage capability. The diagnostic status route (`browser_interface.h:1981-2024`) reports the diagnostic backend's cached `StorageStatus`; with no diagnostic I/O after removal it remains the prior durable state. This explains both the stale diagnostics and the absence of a degraded storage report.

`BOOT_SELF_TEST=PASS` is emitted by `main.cpp:356-357` from the generic `ready`/`bootFailed` startup flags. `ready` is set after configuration startup (`main.cpp:807-840`) and is not a live SD probe. It therefore continued to report PASS after the card was removed and a later payload open proved the medium unavailable.

## Smallest architecture-conformant correction (proposal only)

Add a single owner-level “physical I/O failure” transition, callable by a subordinate only while it holds the existing `PhysicalSdOwner::Lease`. A proven filesystem operation failure (for example `SD.open`, read, write, rename, remove or verification failure after a valid lease) should:

- atomically mark the owner unavailable/degraded;
- make the affected subordinate store unavailable and report its logical degraded/fault state;
- emit one bounded storage-health/fault observation;
- leave Race Engine, authority, Browser transport and committed records unchanged;
- never call `SD.begin()` or remount from the consumer.

The owner remains the only component allowed to call `SD.begin()` through `begin()`/`recover()`. In an eligible `READY`/settled `FINISHED` state, the existing recovery service can then perform exactly one owner-controlled mount and reconstruct History, active-detail and Diagnostics before advertising durable health. A missing card must not auto-recover merely because it is reinserted; recovery remains explicit owner-controlled behaviour.

The first production correction should cover the proven `Esp32SpiSdStore::read()` open/read boundary and the equivalent physical-failure boundaries in the SD diagnostic backend. It should not add a second recovery state machine, alter `max`/race semantics, or make `/history` fabricate a successful result. The HTTP failure remains bounded; the important new result is authoritative unavailable/degraded capability state.

## Required proof after correction

The next software/physical acceptance must prove, without another consumer-side mount:

1. Idle card removal is not falsely reported as a successful current mount; the first real read failure transitions the owner and logical consumers to unavailable/degraded.
2. One missing-medium `/history` read returns a bounded explicit failure, latches the storage-health transition once, leaves sequences 9/10/11 and authoritative RAM state unchanged, and causes no per-loop retry storm.
3. `/health`, diagnostic status and diagnostics download expose the degraded/unavailable capability using their existing bounded semantics; racing, Browser authority and HTTP remain usable.
4. No autonomous `SD.begin()`, remount or retry occurs while the card remains out.
5. Reinsertion alone does not restore durable health. An explicit owner-controlled recovery in eligible `READY`/settled `FINISHED` performs exactly one mount attempt, then reconstructs History/Details/Diagnostics.
6. Recovery restores durable health and reads sequences 9/10/11 consistently, with no duplicate or lost committed record.
7. Recovery remains suppressed in prohibited active lifecycle states and does not change race timing or authority.

## Classification

This is a genuine OPEN WP5 storage-health/recovery defect, not an SD corruption finding and not a Browser/HTTP defect. The accepted SD-2 missing-medium checkpoint is a valid FAIL because the implementation surfaces the endpoint error but fails the required owner/degraded-health transition. No correction has been implemented in this report.
