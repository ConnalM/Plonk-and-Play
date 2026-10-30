# WP6 Recovery, Reset and Customer Firmware Update Contract

**Status:** APPROVED PRODUCT CONTRACT — IMPLEMENTATION PENDING

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

### Reset settings

Reset settings restores installation, configuration and settings to factory
defaults while retaining committed race History/Results/Details, PB, Track
Record and Record Era. It requires controlling authority and explicit
confirmation. It is not a reboot and it is not a full customer-data wipe.

### Erase everything

Erase everything is the separate, genuinely destructive action intended for
resale or a completely fresh start. It erases configuration/settings and all
customer race/record data. It requires controlling authority and substantially
stronger explicit confirmation, then returns to the safe first-boot/
commissioning state. “Factory Reset” must not be used as an ambiguous label for
both actions.

## 2. Customer firmware update

The controller is the customer-facing update authority. Firmware update is a
Browser-only function; the Taster is deliberately a MINIMUG-style race
interface, not an alternative P&P administration, maintenance, Settings or
diagnostics interface. A customer-initiated update must be available through
the normal P&P Browser without development tools. Both supported sources use
the same update boundary:

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
the candidate is validated. The Browser supports both normal online P&P update
and local firmware-file upload for offline/support/recovery use. Both paths
must authenticate and validate legitimate, compatible P&P firmware. Ordinary
customer operation rejects downgrades; a separately controlled support/recovery
mechanism may permit an intentional downgrade when genuinely required. Normal
racing remains independent of Internet availability.

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
  committed as active; automatic rollback to the previous known-good firmware
  is attempted first;
- repeated failed attempts are bounded and retain a bootable known-good path;
- recovery/rollback must not fabricate, delete or rewrite committed race data.

If automatic rollback cannot restore normal operation, P&P enters a
customer-visible recovery/support mode. A failed update reports that the
previous version was restored whenever rollback succeeds.

## 4. Authority and safety

- Read-only status may be viewed by any connected Browser.
- Restart, Reset settings, Erase everything and firmware update require the
  controlling Race Director/Master or an explicitly authorised support path.
- Reset settings, Erase everything and firmware update require controlling
  authority, explicit confirmation and a settled non-racing lifecycle. They are prohibited during
  `STARTING`, `RACING`, `PAUSED`, `RESTARTING` and unfinished settlement.
- Browser/UI code requests operations; it does not own reboot, update, storage,
  rollback or authority state.
- A failed update must not change Race Director ownership or make a Browser
  authoritative.

## 5. Data-retention matrix

| Data | Restart P&P | Reset settings | Firmware update | Failed/interrupted update | Recovery/rollback | Erase everything |
|---|---|---|---|---|---|---|
| Installed known-good firmware | retained | retained | replaced only after validation | retained | restored/retained | retained until the erase completes |
| Network credentials | retained | factory defaults | retained | retained | retained | erased |
| Remembered setup/configuration | retained | factory defaults | retained | retained | retained | erased |
| Committed History/Results/Details | retained | retained | retained | retained | retained | erased |
| PB / Track Record / Record Era | retained | retained | retained | retained | retained | erased |
| Active individual race | not restored after uncontrolled interruption | same | same | same | same | not applicable |
| Incomplete temporary transactions | ignored/reclaimed under PSF rules | same | same | same | same | not applicable |

## 6. Fixed first-boot/recovery outcome

After a successful restart, update commit or rollback, P&P loads last-known-good
configuration, reconstructs committed storage, reconciles equipment and returns
to normal readiness. Reset settings uses the same first-boot/default-settings
path while retaining customer records. Erase everything enters the existing
factory-default commissioning path after the confirmed wipe. An interrupted
individual race is not resumed automatically; an unfinished competition may be
offered for continuation under `SYSTEM_LIFECYCLE.md`.

No product owner decisions remain open in this contract. Implementation must
still define measurable image-authentication, compatibility and post-boot
health checks within these boundaries and submit those details for technical
review; they are not permission to change the product decisions above.

