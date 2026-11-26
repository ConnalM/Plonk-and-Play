# WP8 S4b — independent role build definitions

This S4b increment introduces build-definition scaffolding only.  It does not
create a Core or Taster firmware image, select a board, assign pins, activate
the Taster link, provision credentials, or replace the accepted integrated
firmware.

`PP_S4_BUILD_ROLE` is a mutually exclusive cache selection with three values:

| Value | Status | Purpose |
| --- | --- | --- |
| `INTEGRATED` | default and only linkable profile | preserves the accepted ESP-IDF development composition |
| `CORE` | fail-closed scaffold | declares Core sources, components and zero display assets |
| `TASTER` | fail-closed scaffold | declares Taster presentation components/assets without Core authority/storage/HTTP components |

Core and Taster deliberately stop before ESP-IDF project configuration. Their
sources, requirements, assets and identities are provisional declarations only:
they are not consumed by an independent target. Their only listed sources are
the independently verified compile probes, not application entry points. No
role may silently select the integrated graph. The missing board-neutral entry
point, service extraction, explicit project-component control and Arduino
transitive-dependency audit are prerequisites for a later, separately
authorised target compile/link increment.

## Fixture and capacity guard

The two future-role manifests *declare* acceptance simulation, simulated
racing, the Phase 8B P1 test, test update-key and physical-racing-test
definitions forbidden. This is not enforcement: an enforcement mechanism must
be in place before either role is enabled. Instrumentation and debug-only flags
remain for a later audit. The S4 contract remains seven HTTP clients, four SSE
clients, a twelve-socket architectural floor and thirteen-socket bare
admission floor. These are design minima, not a measured production margin.

## Windows command-length proposal

Response files are a future candidate, not an enabled or established solution.
A separately authorised disposable role build must validate them with the
actual installed CMake, Ninja and ESP toolchain; it must inspect generated
commands and any response files before making a target-build claim. If that
combination does not use response files for the long Arduino/IDF command, the
fallback is a reproducible short-path disposable checkout/build root. Neither
route may alter the SDK, global toolchain or accepted build tree.
