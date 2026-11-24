# WP8 S3b: post-handshake Taster wire protocol, first host slice

Owner-approved transport: dedicated TCP over Wi-Fi for one paired Taster. Reliability, automatic recovery and no detrimental impact to SMUGLAND outrank speed. See Issue #14, S3a PR #17 and the reviewed S3b PR. This is a host protocol slice, not a live transport or production security implementation.

## Format

Every record is a 32-byte header, an exactly sized payload, and a full 32-byte HMAC-SHA256 tag over header and payload. All integers are unsigned big-endian. Header offsets:

| Offset | Width | Field |
|---|---:|---|
| 0 | 4 | Magic PPTL (50 50 54 4c) |
| 4 | 1 | Version 1 |
| 5 | 1 | Kind |
| 6 | 2 | Payload byte count |
| 8 | 8 | Nonzero Core boot epoch |
| 16 | 8 | Nonzero fresh link identity |
| 24 | 8 | Direction-local sequence (starts at 1, never wraps) |

| Kind | Direction | Payload bytes | Fields, in order (byte widths) |
|---|---|---:|---|
| 1 Command | Taster -> Core | 49 | ID(8), authority generation(8), lifecycle revision(8), race ID(8), proposal revision(4), Core expiry us(8), action(1), mode(1), active setup value(2), confirmed(1) |
| 2 Result | Core -> Taster | 9 | Command ID(8), gate status(1) |
| 3 ControlState | Core -> Taster | 38 | Authority generation(8), lifecycle revision(8), race ID(8), proposal revision(4), lifecycle(1), Taster allowed(1), Core sample time us(8) |
| 4 Ping | Taster -> Core | 16 | Nonzero token(8), Taster send us(8) |
| 5 Pong | Core -> Taster | 32 | Token(8), echoed Taster send us(8), Core receive us(8), Core send us(8) |
| 6 DisplayState (S3c) | Core -> Taster | 322 | Atomic fixed two-lane snapshot; layout below |

Enum assignments are the explicit sequential S3a enum values at the reviewed baseline; booleans are only 0 or 1. Payload lengths are exact, not native C++ object layouts. Unknown versions/kinds, bad tags, wrong direction/session, out-of-sequence records and noncanonical fields are fatal to that connection. The decoder is bounded (386 bytes of frame storage after S3c), does not scan for magic, and exposes only completely verified records. The transport must close on fatal error/EOF with partial frame, enforce bounded partial-read/write time and limit unauthenticated clients.

## Semantics and gates

- Separate authenticated direction keys scoped to a fresh link are a precondition. The host test uses real OpenSSL HMAC and constant-time tag verification with deterministic test-only keys. [S3d host handshake and key derivation](WP8_S3D_TASTER_HANDSHAKE.md) define a prototype for previously paired devices; production provisioning, secure storage, ESP cryptographic provider and live authentication remain **not implemented**. MAC is authentication/integrity, not encryption. No secrets or real network traffic in this slice.
- The wire sequence is distinct from the command ID. A repeated identical command can arrive with a new frame sequence and returns the gate's cached state; a repeat of the original frame sequence is fatal. IDs never replay across sessions. Receipt/queued/in-flight are not Race Control acceptance. Gate remains a prototype; actual result mapping and execution-time revalidation are required before live integration. Command value ranges and sporting guards remain in the S3a gate.
- ControlState is command-facing context, not a display snapshot. S3c adds DisplayState for the current fixed two-lane renderer; explicit remote display health, future auxiliary/specialist schemas and actual renderer integration remain open. Ping/pong carries timestamps only; offset estimation, uncertainty and deadline policy are unimplemented. Full frame replay/authentication tests do not establish a secure handshake.
- No existing production files, Browser routes, socket limits, firmware environments, device state or records changed. Existing S2 and S3a gates remain. Current XIP configuration permits 10 sockets; stated HTTP capacity plus one dedicated TCP listener and peer needs at least 12 before margin. Prove existing SMUGLAND capacity, race-loop responsiveness, heap and reconnect recovery on the eventual Core; do not reduce Browser capacity or increase limits speculatively.

## S3c — fixed Taster display snapshot (host prototype)

The Core owner loop must collect a coherent Noticeboard / proposal / readiness / committed preference / command-context sample and Core monotonic timestamp. The projection rejects inconsistent revisions/lifecycle and more than two lanes rather than truncating or guessing. The entire sample is authenticated as one frame; no independent lane records can be mixed. The receiver must not treat a bare ControlState as a fresh race display. No live calls, interpolation, stale-link detection, UI, pairing, execution fence or socket integration here.

DisplayState layout (offsets relative to payload start, big-endian integers, microseconds for all time values):

| Offset | Bytes | Fields |
|---:|---:|---|
| 0 | 36 | authority generation(8), lifecycle revision(8), race ID(8), proposal revision(4), Core sample time(8) |
| 36 | 14 | lifecycle, allowed, active mode, proposed mode, readiness, active finish, proposed finish, restart method, phase, red count, red lit, start signal, entry count, proposed lanes (1 each) |
| 50 | 14 | proposed laps(4), active laps(4), proposed minutes(2), active minutes(2), flags(2) |
| 64 | 72 | elapsed, remaining, overtime, scheduled GO, phase until, pause, restart, expiry, finish until (8 each) |
| 136 | 8 | preference publication revision supplied by Core (not a separate storage generation) |
| 144 | 52 | two preference slots: graphic(1), NUL-terminated zero-padded label(25) |
| 196 | 126 | two lane slots (63 each): physical lane(1), flags(1), gap kind(1), laps/classified/penalty/position/laps behind (4 each), last/best/finish/overtime/gap time (8 each) |

Explicit enums and bit masks are defined in `taster_link_display_payload.h`; native enum values are compile-time checked in `taster_link_display_projection.h`. Unused lane slots are canonical all-zero; blank labels retain existing generic fallback; absent committed preferences have explicit presence bit clear, zero revision/labels and default graphics. Unknown gaps and lap/overtime presence are not inferred from numeric zero. Shared lap target is 32-bit, not clamped to Taster's local 999 adjustment limit. Practice remains display-only: Taster authority must be false whenever either the active or proposed mode is Practice, including READY. Restarting and faulted lifecycle also prohibit Taster authority. The payload does not include private racer catalog, sensor identities, storage records or the future configurable auxiliary/Timed Stage/Drag display fields. Those require their separately gated schemas. This is a pre-production version-1 extension; earlier prototype readers reject unknown kind 6 rather than silently interpreting it. Version/capability negotiation must be fixed before shipping.

The host test validates projection from native fixture fields, every two-chunk split, round trip, authenticated malformed fields, wrong direction, tamper/replay, role and lifecycle transitions, no truncation and presence semantics. It does not certify live coherent sampling, display rendering, race timing or wireless load.
