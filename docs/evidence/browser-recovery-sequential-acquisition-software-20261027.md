# Browser recovery sequential-acquisition software evidence

Implementation under review: `88515b8` (`fix: serialize Browser recovery acquisition`).

## Correction

The generated Normal Browser recovery path now uses one bounded acquisition
chain per Browser:

`/state` → `/proposal` → `/context`

The chain is selected only for `reconnect` and `recovery` triggers.  Healthy
SSE-driven operation keeps its existing behaviour, including the existing
2/4/8/16-second SSE retry schedule.  Recovery still derives authority solely
from fresh `/context`; no authority, race, persistence, HTTPD limit, SSE-slot,
ordinary-reserve, or server-admission semantics changed.

The existing EventSource/epoch ownership was retained.  The deterministic
reboot-shaped model proves that a stale generation cannot deliver to a
replacement and that each Browser has at most one current recovery stream and
one recovery acquisition request at a time.

## Deterministic proof

`acceptance/stage14c/browser_reboot_recovery_runtime.js` models two Browser
identities (Master and spectator), simultaneous controller-generation loss,
sequential recovery acquisition, a transient send failure on one SSE, a
healthy peer SSE, the clients 8/9/10 admission boundary, explicit 503/close
rejection, capacity release, fd/generation staleness, fresh reconstruction,
role preservation, baseline reclamation, and absence of race/persistence
mutation.

Result:

`Stage 14C two-Browser reboot recovery PASS`

## Software checks

Passed:

- all Stage 14C Browser runtime scripts, including liveness, admission/recovery,
  notification, setup synchronisation, Results, resume, presentation, start
  presentation, normal-poll guard, and the new reboot-shaped recovery model;
- `acceptance/stage10/http_sse_isolation_contract.py`;
- `acceptance/stage10/http_sse_isolation_deterministic.py`;
- Stage 15 network contract, control-lease contract, WP3 Browser/diagnostics
  contracts, WP4A diagnostic durability, and bounded multi-client SSE contract;
- Stage 15 WP3 sequence/production/export/Browser-observation contracts;
- Persistent Storage Foundation/WP2 suites (`acceptance/persistent_storage/run.ps1`).

The Stage 14C integration review's binary-content checks were not runnable in
this worktree because the stage14c acceptance/demo binaries were not present;
the migration-era PlatformIO build command is also unavailable here due the
workspace's existing platform/lock permission condition.  No physical artifact
was built or flashed for this change.

## Scope audit

No production server limits or admission arithmetic changed.  No normal
polling loop was added, no global transport lock was introduced, and no
authority, race, persistence, AP/STA, or Browser product semantics were
changed outside recovery acquisition ordering.
