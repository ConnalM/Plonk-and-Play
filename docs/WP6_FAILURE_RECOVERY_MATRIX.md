# WP6 Deterministic Failure / Recovery Matrix

This matrix is the software-first proof plan for the WP6 contract. Each row
must produce bounded state, firmware identity, data-retention and diagnostic
evidence. Physical proof is reserved for rows marked **Hardware**.

| Case | Initial state | Action/fault | Expected boot/recovery state | Active firmware | Customer data | Evidence | Auto/Wokwi | Hardware |
|---|---|---|---|---|---|---|---|---|
| Restart | READY, known-good image | authorised restart | normal boot, READY | same | all committed data retained | request, reboot marker, provenance, reconstruction | Yes | Later |
| Reset | READY, known-good image | authorised non-destructive reset | same as Restart | same | same | reset class and post-boot state | Yes | Later |
| Factory reset | READY, settled | confirmed factory reset | commissioning/default state | same | per owner-selected scope | confirmation, erase report, first-boot marker | Yes | Later |
| Invalid image | READY | upload invalid identity/format | reject before staging/install | old | unchanged | bounded rejection and unchanged provenance | Yes | No |
| Incompatible image | READY | incompatible board/schema/version | reject before install | old | unchanged | compatibility reason | Yes | No |
| Truncated/corrupt upload | READY | cut/corrupt transfer | staged candidate rejected | old | unchanged | transfer/integrity evidence | Yes | Later |
| Insufficient staging | READY | staging capacity exhausted | bounded failure | old | unchanged | capacity diagnostic | Yes | Later |
| Power loss during upload | staging | interrupt transfer | old image boots | old | unchanged | incomplete upload ignored | Simulated; physical later | Yes |
| Power loss before install | candidate staged | interrupt before commit | old image boots | old | unchanged | pending candidate not active | Yes | Later |
| Power loss during install | install in progress | interrupt write/boot transition | old image or bootloader recovery | old/recovery | unchanged | boot-selection and rollback marker | Simulated; physical later | Yes |
| New image fails to boot | candidate selected | boot failure/watchdog | rollback/recovery | old | unchanged | failure count and rollback | Yes | Later |
| New image fails health | candidate boots | health confirmation fails | rollback/not committed | old | unchanged | health-gate failure | Yes | Later |
| Repeated failures | repeated candidate faults | retry exhaustion | bounded support/recovery mode | known-good | unchanged | no retry storm; explicit state | Yes | Later |
| Successful update | READY | valid candidate | candidate health-confirmed and committed | new | retained | old/new provenance and commit marker | Yes | Later |
| Recovery/rollback | failed candidate | boot fallback | known-good READY | old | retained | rollback reason and reconstruction | Yes | Later |
| Interrupted race after restart | RACING | uncontrolled reboot | READY or offered competition continuation | same | completed prior records retained; active race not official | no fabricated History | Yes | Later |
| Factory reset confirmation failure | READY | cancel/unauthorised request | unchanged READY | same | unchanged | rejection and no erase | Yes | No |

## Required automated assertions

The deterministic suite must assert that update requests respect authority and
lifecycle, candidates are validated before activation, the known-good slot is
never discarded early, every failure leaves committed customer data readable,
and rollback/recovery is bounded and idempotent. Wokwi/fake-backend tests should
inject truncation, checksum failure, power-loss-at-boundary and boot-health
failure without adding a production polling path.

## Later physical proof

After the software matrix passes, hardware acceptance must cover at least one
real upload, interruption during upload/install, failed-candidate boot or
rollback, successful update, restart/reset, and customer-data retention. The
exact physical sequence waits for the owner decisions in the contract.

