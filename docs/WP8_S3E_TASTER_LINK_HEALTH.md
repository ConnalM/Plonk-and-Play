# WP8 S3e — link health and recovery host policy

A paired, authenticated, direction/sequence-verified session is a prerequisite. The fixed Taster client owns no race clock, engine or storage. This slice does not attach to the existing UI or firmware, open sockets, or change the Core/Browser. It is a host policy for the later remote adapter.

## Freshness and clock domains

For each matched ping/pong, t0/t3 are Taster send/receive monotonic microseconds, c1/c2 Core receive/send monotonic microseconds. With a nonnegative one-way delay and relative rate difference bounded by d=200 ppm, Core now lies conservatively between:

- lower = c2 + max(0, now-t3-ceil((now-t3)*d/1e6))
- upper = c1 + now-t0+ceil((now-t0)*d/1e6)

Use checked addition; reject time reversal, impossible processing intervals, overflow, disjoint subsequent clock observations, mismatched token/echo/session, timed-out ping, expired mapping. No symmetry or exact offset assumption. Production must verify the relative drift bound, timestamp points and clock behavior; no accuracy claim from a host fixture.

Initial engineering values in Policy (not frozen sporting tolerances): startup 2 seconds, ping timeout 250 ms, maximum snapshot age 500 ms, clock mapping 5 seconds, pending result 2 seconds. The owner loop must refresh the clock before expiration and call tick even in silence. Revisit these values with wireless load and responsiveness measurements; reliability and existing SMUGLAND capacity take priority.

Only a complete authenticated DisplayState after the first matched pong (Core sample time at or after Core pong-send) enables a current view. Initial pre-pong samples are ignored. Sample time must not regress; age is conservatively upper minus sample time and must be strictly below the limit. Future samples beyond upper are invalid. A pong, result or bare ControlState does not refresh the display. Expired, malformed or inconsistent state retires the link; a delayed frame cannot revive it. The next attempt needs a fresh authenticated session. The transport must keep the pong/snapshot publication order and prevent unbounded queues; the health model assumes decoder validation and accurate timestamps.

## Display and input

Offline, synchronising, fresh and stale are explicit states. Only fresh data may appear as live; retained same-boot information must be labelled stale/synchronising in the existing message strip. On stale/link loss, suppress all lights, freeze sampled time values, disable controls, discard gestures and pending command knowledge. New Core boot clears the retained race entirely. Even while fresh, GO is never invented from a red countdown or interpolated past its possible validity. The prototype holds sampled timers/lights, not local timing progression; real renderer cadence and visual continuity require physical evidence.

The renderer must respect View.mayControl and readyForNewTouch, not the raw cached snapshot's authority alone. An interaction revision changes whenever a gesture is invalidated: cancel hold, preview, confirmation and pending local UI state as applicable. The actual contact must return to neutral and a genuinely new down/up is required. Do not simulate release on cancellation. Authority generation, lifecycle/race/proposal revision, readiness and mode changes cancel the held interaction; later return of control does not revive it. Practice stays display-only. The existing 700 ms pause hold and confirmation rules remain the renderer's responsibility. No changes to the integrated Taster model or gestures are made here.

One fresh command ID may be registered on a permitted release; the returned context is not an authoritative result or permission to bypass the Core execution fence. A matching queued/in-flight result cannot extend the deadline; a matching terminal result retires pending state without altering the race snapshot. Missing/uncertain reply, send failure, disconnect or takeover never trigger retransmission or claim that the command failed to execute. Indicate uncertain outcome in the existing status area and reconcile from fresh authoritative information. The adapter explicitly acknowledges the indication after surfacing it, not as a sporting result. Fresh input and fresh snapshot are required after reconnect; the Core race and SMUGLAND must continue undisturbed.

## Remaining integration and review

Real secure provisioning, ESP RNG/MAC provider, authenticated socket reader/writer and reconnect scheduling/backoff, actual snapshot publisher, clock/touch/renderer integration, genuine physical neutral detection and status wording, request/result mapping and execution-time fencing remain open. No automatic retry, no real race commands, no firmware build, no physical or SMUGLAND capacity acceptance. The host test exercises conservative bounds, asymmetric-delay allowance, invalid observations, mapping refresh/expiry, snapshot expiry even with heartbeat, GO suppression, stale and new-boot behavior, Browser takeover/release, proposal changes, pending uncertainty, replay rejection and Practice. Existing S3a-d host regressions are retained.
