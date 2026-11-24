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

Enum assignments are the explicit sequential S3a enum values at the reviewed baseline; booleans are only 0 or 1. Payload lengths are exact, not native C++ object layouts. Unknown versions/kinds, bad tags, wrong direction/session, out-of-sequence records and noncanonical fields are fatal to that connection. The decoder is bounded (128 bytes of frame storage), does not scan for magic, and exposes only completely verified records. The transport must close on fatal error/EOF with partial frame, enforce bounded partial-read/write time and limit unauthenticated clients.

## Semantics and gates

- Separate authenticated direction keys scoped to a fresh link are a precondition. The host test uses real OpenSSL HMAC and constant-time tag verification with deterministic test-only keys. Production provisioning, handshake, key derivation/storage and ESP cryptographic provider are **not implemented**. MAC is authentication/integrity, not encryption. No secrets or real network traffic in this slice.
- The wire sequence is distinct from the command ID. A repeated identical command can arrive with a new frame sequence and returns the gate's cached state; a repeat of the original frame sequence is fatal. IDs never replay across sessions. Receipt/queued/in-flight are not Race Control acceptance. Gate remains a prototype; actual result mapping and execution-time revalidation are required before live integration. Command value ranges and sporting guards remain in the S3a gate.
- ControlState is command-facing context, not the full race-display snapshot. Lane results, countdown/timer/phase, preference revisions, readiness and explicit remote display health still require a separate reviewed schema and integration. Ping/pong carries timestamps only; offset estimation, uncertainty and deadline policy are unimplemented. Full frame replay/authentication tests do not establish a secure handshake.
- No existing production files, Browser routes, socket limits, firmware environments, device state or records changed. Existing S2 and S3a gates remain. Current XIP configuration permits 10 sockets; stated HTTP capacity plus one dedicated TCP listener and peer needs at least 12 before margin. Prove existing SMUGLAND capacity, race-loop responsiveness, heap and reconnect recovery on the eventual Core; do not reduce Browser capacity or increase limits speculatively.
