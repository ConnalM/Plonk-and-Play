# WP6 Recovery, Reset and Customer Firmware Update Contract

**Status:** DOCS-FIRST WORKING CONTRACT — OWNER DECISIONS MARKED

**Purpose:** define the customer-safe recovery, reset, factory-reset and firmware-update
boundary for WP6 without changing race, authority or persistent-record semantics.

## 1. Terms and fixed behaviour

### Restart

Restart is an orderly or externally requested controller reboot into the same
installed firmware. It may be triggered by an authorised support action, a
software health action, or an ordinary hardware reset/reboot boundary. Restart:

- preserves committed configuration, network credentials, remembered authority,
  History, Results/Details, PB, Track Record and Record Era;
- does not restore the volatile state of an interrupted individual race;
- reconstructs the last-known-good persistent state and returns to the normal
  startup/readiness flow;
- treats any incomplete transaction as uncommitted debris, never as official
  History.

These rules follow `SYSTEM_LIFECYCLE.md` and the accepted Persistent Storage
Foundation design.

### Reset

Reset is a deliberate restart boundary distinguished from Factory Reset: it
does not erase customer data or installed firmware. It must leave the same
persistent state and data-retention outcome as Restart. The exact customer
surface and whether the product exposes a separate named Reset action remain an
owner decision (see §10); firmware must not silently make Reset destructive.

### Factory Reset

Factory Reset is destructive and requires controlling Race Director/Master
authority plus explicit confirmation. It returns the controller to a safe
first-boot/commissioning state. The exact deletion scope is **OWNER DECISION
REQUIRED**; no implementation may assume it.

## 2. Customer firmware update

The controller is the customer-facing update authority. A customer-initiated
update must be available through the normal P&P interface without development
tools. The update boundary is:

```text
authorised request
  -> receive/stage candidate
  -> validate image identity, integrity, compatibility and version policy
  -> mark candidate pending
  -> install through recoverable boot/update mechanism
  -> reboot into candidate
  -> perform boot/health confirmation
  -> commit candidate as active, or roll back to known-good
```

The replacement must never destroy the only bootable known-good firmware before
the candidate is validated. Normal racing remains independent of Internet
availability. The exact initiation UX, image source/authentication policy,
downgrade policy and confirmation timeout are **OWNER DECISION REQUIRED**.

## 3. Failure and recovery contract

The following behaviours are fixed:

- invalid, incompatible, truncated or corrupt images are rejected before
  installation;
- insufficient staging space is an explicit bounded failure and leaves the
  active firmware/data unchanged;
- power loss during upload or before installation leaves the active firmware
  and committed customer data unchanged;
- power loss during installation must boot the previous known-good image or a
  bootloader recovery path;
- a candidate that fails to boot or fails post-boot health confirmation is not
  committed as active;
- repeated failed attempts are bounded and retain a bootable known-good path;
- recovery/rollback must not fabricate, delete or rewrite committed race data.

The exact customer-visible recovery screen, retry count and service/manual
fallback are **OWNER DECISION REQUIRED**.

## 4. Authority and safety

- Read-only status may be viewed by any connected Browser.
- Restart/Reset requires the controlling Race Director/Master or an explicitly
  authorised support path.
- Factory Reset and firmware update require controlling authority, explicit
  confirmation and a settled non-racing lifecycle. They are prohibited during
  `STARTING`, `RACING`, `PAUSED`, `RESTARTING` and unfinished settlement.
- Browser/UI code requests operations; it does not own reboot, update, storage,
  rollback or authority state.
- A failed update must not change Race Director ownership or make a Browser
  authoritative.

## 5. Data-retention matrix

| Data | Restart | Reset | Firmware update | Failed/interrupted update | Recovery/rollback | Factory Reset |
|---|---|---|---|---|---|---|
| Installed known-good firmware | retained | retained | replaced only after validation | retained | restored/retained | **OWNER DECISION REQUIRED** |
| Network credentials | retained | retained | retained | retained | retained | **OWNER DECISION REQUIRED** |
| Remembered setup/configuration | retained | retained | retained | retained | retained | **OWNER DECISION REQUIRED** |
| Committed History/Results/Details | retained | retained | retained | retained | retained | **OWNER DECISION REQUIRED** |
| PB / Track Record / Record Era | retained | retained | retained | retained | retained | **OWNER DECISION REQUIRED** |
| Active individual race | not restored after uncontrolled interruption | same | same | same | same | not applicable |
| Incomplete temporary transactions | ignored/reclaimed under PSF rules | same | same | same | same | not applicable |

## 6. Fixed first-boot/recovery outcome

After a successful restart, update commit or rollback, P&P loads last-known-good
configuration, reconstructs committed storage, reconciles equipment and returns
to normal readiness. A first-ever or explicitly factory-reset boot uses the
existing factory-default commissioning path. An interrupted individual race is
not resumed automatically; an unfinished competition may be offered for
continuation under `SYSTEM_LIFECYCLE.md`.

## 7. Owner decisions required before implementation

1. **Factory-reset scope:**
   - A: erase installation/configuration only; retain race records (safer,
     preserves customer history);
   - B: erase configuration and all customer race records (true data reset,
     destructive); or
   - C: two separately confirmed actions (configuration reset vs full data
     erasure).
2. **Update initiation:** Browser Settings action, local Taster action, or both.
3. **Image trust/source:** signed online image only, locally staged image, or
   both; required version/compatibility and downgrade policy.
4. **Recovery presentation:** automatic rollback with a bounded retry, or a
   customer-visible recovery/support mode after the first failed candidate.
5. **Reset naming:** whether “Reset” is exposed separately from “Restart” or is
   an internal/support term only.

No implementation should silently choose among these options.

