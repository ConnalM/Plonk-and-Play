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

Core and Taster deliberately stop before ESP-IDF project configuration.  Their
only listed sources are the independently verified compile probes, not an
application entry point.  No role may silently select the integrated graph.
The missing board-neutral entry-point and service extraction is an explicit
prerequisite for a later, separately authorised target compile/link increment.

## Fixture and capacity guard

The two future-role manifests forbid acceptance simulation, simulated-racing,
test update-key and physical-racing-test definitions.  They retain the S4
contract: seven HTTP clients, four SSE clients, a twelve-socket architectural
floor and thirteen-socket bare admission floor.  These are design minima, not
a measured production margin.

## Windows command-length proposal

`PP_S4_FORCE_NINJA_RESPONSE_FILES=ON` is a later-role-build opt-in only.  It
requires the Ninja generator and requests CMake's supported
`CMAKE_NINJA_FORCE_RESPONSE_FILE` behaviour before project generation.  The
default is `OFF`, so the accepted integrated build is unchanged.

Before any claim of success, a separately authorised target build must use a
fresh disposable role directory, retain the approved SDK/configuration inputs,
inspect generated Ninja commands and response files, compile the intended role
objects, then establish whether the full link is viable.  If the installed
CMake/Ninja combination does not use response files for the long Arduino/IDF
command, the fallback is a reproducible short-path disposable checkout/build
root; no SDK, global toolchain or accepted build tree may be altered.
