# WP8 S3d — authenticated session handshake (host prototype)

**Scope:** owner-approved dedicated TCP/Wi-Fi direction, one previously paired Taster. Implements and tests a bounded handshake state machine using established cryptographic primitives, not production commissioning or network integration. Do not advertise as a production security review.

## Preconditions and session lifecycle

Core and Taster have the same unique 256-bit CSPRNG-generated pair secret and nonzero 128-bit pair identifier, supplied from a future approved provisioning/storage mechanism. No default/shared fleet secret, user password, SSID trust or unauthenticated first-use enrolment. The pair identifier is public. Both endpoints require a trustworthy CSPRNG. Core creates a fresh nonzero boot epoch each boot and owns one monotonically increasing link allocator for that epoch; allocate and retire an ID even if a handshake subsequently fails. Never reset/reuse an allocator during a boot; exhaustion fails closed. Fresh 256-bit nonces are required per attempt. All-zero guards detect invalid values, not entropy quality. Epoch uniqueness and entropy are production obligations, not proved by host tests.

## Fixed messages

Six-byte prefix: ASCII PPTH, version 1, kind byte. All integers big-endian; exact message lengths; no optional fields or negotiation downgrade. These messages precede the authenticated PPTL application records.

| Kind | Direction | Total bytes | Fields following prefix |
|---|---|---:|---|
| 1 Hello | Taster to Core | 54 | pair ID (16), Taster nonce (32) |
| 2 Challenge | Core to Taster | 86 | Core boot epoch (8), link ID (8), Core nonce (32), Core proof (32) |
| 3 Proof | Taster to Core | 38 | Taster proof (32) |
| 4 Finish | Core to Taster | 38 | Core finish proof (32) |

Transcript T = exact 54-byte Hello + first 54 bytes of Challenge (including both prefixes, version, pair identity, nonces and session). RFC5869 HKDF-SHA256: IKM = pair secret; salt = T; info = literal ASCII label below (no terminating NUL); output = 32 bytes per purpose. Proofs are full HMAC-SHA256 tags over T using the respective derived key.

| Purpose | HKDF info |
|---|---|
| Core challenge | P&P PPTH v1 core challenge |
| Taster proof | P&P PPTH v1 taster proof |
| Core finish | P&P PPTH v1 core finish |
| Taster to Core records | P&P PPTL v1 Taster to Core |
| Core to Taster records | P&P PPTL v1 Core to Taster |

Core verifies pair ID, allocates fresh link, generates nonce, issues challenge. Taster authenticates challenge against its own Hello before returning proof. Core verifies proof before sending Finish. Core exposes keys only after the entire Finish is sent; Taster exposes keys only after validating Finish. Sending is not confirmation of peer receipt. If connection fails during this window, retire the session; no automatic command retry. Application frame sequences restart at one under new keys and identity. Wrong order, wrong version/shape, bad proof, provider failure or abort poisons the instance and wipes derived keys. Provider must outlive the handshake and provide constant-time comparison and non-elidable zeroization. A transport-owned MAC provider must retire its own copies on disconnect. Do not alias input and output packets.

## Explicitly open

- Provisioning/replacement UX, secret generation and protected persistent storage, ESP crypto/RNG provider, production review of protocol and threat model. The PSK authentication provides integrity, not confidentiality or forward secrecy. Do not use low-entropy passwords; recorded exchanges could verify guesses. Evaluate confidentiality and use a reviewed standard secure transport if required before release.
- TCP reader/writer must collect exactly the expected fixed handshake message, preserve coalesced bytes for next phase, reject wrong lengths/order, enforce bounded read/write deadlines and unauthenticated-client limits. Neither an incoming Hello nor incomplete handshake can evict a valid live Taster. Only verified replacement can be considered by the Core owner; close/disconnect and cancel old pending commands before activating a replacement. No application command accepted before mutual authentication and fresh authoritative snapshot. Race and SMUGLAND continue unaffected.
- Actual clock/freshness/health, UI gesture cancellation, command result mapping and actual execution fence, owner-loop snapshot publication, socket/heap and full SMUGLAND coexistence measurement, separate builds and physical acceptance.
- No production calls, credentials, sockets, firmware build, hardware or storage changes. The host test uses OpenSSL, independent fixed vectors, simulated negative cases and a CSPRNG smoke; deterministic fixture keys are never shipped.

Related: [S3b/S3c record protocol](WP8_S3B_TASTER_WIRE_PROTOCOL.md), [Master Project Plan](MASTER_PROJECT_PLAN.md), [governing design](PP_DESIGN_SPEC.md#core-and-optional-wireless-taster).
