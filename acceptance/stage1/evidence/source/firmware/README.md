# P&P Stage 1 — ESP32 skeleton

Stage 1 is **ACCEPTED**. It passed the frozen acceptance campaign defined by
`docs/ACCEPTANCE_TESTS_STAGE_1.md` at
`ac6c73bce1fbfad374eee7409b974663647ce65a` on 30 September 2026. The campaign
also demonstrated that its own known-wrong expectation fails before the correct
expectation is restored. Retained evidence and its repeatable harness are under
`acceptance/stage1/`; generated firmware images remain local-only.

The implementation correction made during that campaign exposed System Time as
the shared `pp::systemTime()` service. It preserves the existing ESP32 monotonic
microsecond source and does not alter Stage 1 product behaviour.

This directory is the real firmware skeleton. `experiments/wokwi-plumbing`
remains a separate disposable transport experiment. Accepted specification files
are unchanged. No race logic, session creation, detector simulation, browser
interface, networking or output operation is introduced in Stage 1.

## What exists

- Controller System Time uses `esp_timer_get_time()`: 64-bit monotonic microseconds
  within a boot. It is independent of wall-clock, serial, network and Wokwi.
- Memory owns NVS access behind a replaceable Store interface. The versioned
  Stage 1 configuration record uses explicit byte encoding, a checksum and two
  alternating generations. A failed write preserves the previous valid record.
  Missing, unusable and unavailable storage are distinguished in diagnostics;
  safe defaults are available in working RAM without inventing device assignments.
- Lifecycle requests configuration through the common Message Bus; Memory replies
  through the same bus. The resulting configuration is copied into working RAM.
  Normal idle operation does not poll persistent storage. No configuration edit
  interface exists yet, so normal boots do not rewrite defaults into flash.
- Proposed defaults: two lanes, Lap Race, ten laps, optional race features off,
  sound and power if available. No equipment is claimed available, no physical
  roles are guessed, and skeleton IDLE does not mean a race can start.
- Input, Race Control, Race Engine, Output and Presentation have separate dormant
  endpoints. Registry, Session Definition and configuration are not extra modules.
  Discovery and actual device operation start in later stages.
- Diagnostics are bounded (64 lines, 160 bytes each), UART writes use available
  space only, and idle status is limited to once per ten seconds. Saturation drops
  diagnostic lines with an observable counter rather than waiting for serial.
  Turning diagnostics off does not disable startup or the bus self-test.

## Bus implementation decisions

The in-process bus has twelve participant slots and eight copied messages per
participant mailbox. Trusted boot composition assigns roles; publishers cannot
override Source using a message field. Publication never invokes consumer code.
Subscribers poll their own mailboxes. A full required mailbox rejects the whole
publication explicitly; there is no partial fan-out or silent loss.

Stage 1 is single-threaded/cooperative; this API is not ISR/thread safe. Future
adapters must enqueue through the appropriate synchronised boundary. No timing-
critical race delivery contract is claimed before the input/race proving stages.
Mailbox capacities and policy implementation are changeable implementation choices,
not product limits. Slow future network/storage work must remain outside critical
processing, with per-message semantics defined when those messages are introduced.

The implemented internal contracts are deliberately narrow:

| Type | Publisher | Subscribers | Meaning |
|---|---|---|---|
| LoadConfiguration | Lifecycle | Memory | Boot-time request; nonzero correlation required |
| ConfigurationLoaded | Memory | Lifecycle | Correlated configuration snapshot and load status |
| DiagnosticProbe | Diagnostics | Diagnostics | Development-only test token, time and ID; no product/race meaning |

These are in-process encodings, not new public wire protocols. Permission checks
do not decide race validity or grant external-client authority. Endpoints are
trusted internal handles, not a security boundary against hostile C++ code.
The bus self-test uses the **running common bus**, two authorised diagnostic
subscribers and an unauthorised Input endpoint; it emits no fake INPUT_EVENT,
GO or LAP_COMPLETED and does not implement Stage 2/3 ahead of their scope.

## Build and automated Wokwi tests

From this directory in PowerShell:

```powershell
./build.ps1 -Environment esp32dev
./build.ps1 -Environment verification
./build.ps1 -Environment quiet
./test.ps1 -Environment verification
./test.ps1 -Environment esp32dev -Scenario tests/serial.yaml
./test.ps1 -Environment quiet -Scenario tests/quiet.yaml
./tests/assert-logs.ps1
```

Uses existing PlatformIO 6.1.18 / Espressif32 6.9.0 / Arduino ESP32 2.0.17 and
official Wokwi CLI 0.27.1. No additional libraries or installs are required.
The test launcher consumes an existing process token or the previously configured
user-encrypted `.wokwi/cli-token.clixml`; no credential belongs in Git. Run it as
the Windows user who encrypted that file. All generated firmware/logs are ignored
under `.pio/`. CLI/cloud simulation is sufficient for Stage 1; there is no network
product interface to exercise through the Private Gateway yet.

The verification image uses separate `pp-selftest` and `pp-test-phase` NVS
namespaces and intentionally resets once. It tests the actual Memory adapter and
codec across that reset. The normal `pp-stage1` namespace is not changed by tests.
Fake-storage failure/corruption tests supplement this: simulation cannot establish
physical flash/power-loss reliability. There is no live race to persist or resume.

## Results — 30 September 2026

All three PlatformIO builds and all three Wokwi runs passed. The user also
manually verified startup diagnostics, the Message Bus self-test, `STAGE1_PASS`
and the ten-second IDLE heartbeat in the Wokwi browser before accepting Stage 1.

- Bus: publication/subscription permission, source stamping, identical fan-out,
  64-bit relevant-time and ID preservation, FIFO delivery, no extra delivery,
  atomic queue-full rejection and recovery.
- Memory: missing/default configuration, save/restore, RAM snapshot isolation,
  failed and torn writes, last-good recovery, newest generation selection,
  corrupt-newest fallback, both-invalid defaults, invalid config rejection,
  unavailable storage, and actual NVS configuration surviving an ESP32 reset.
- Normal firmware: boot PASS; status command; repeat self-test; quiet mode;
  suppressed self-test output while quiet; resume; self-test again.
- Quiet-start firmware: reached IDLE with startup diagnostics disabled, then
  exposed status and passed the same self-test after diagnostics were enabled.
- Log assertions verify suppressed output, test counts and increasing System Time.

Evidence: `.pio/verification-serial.log`, `.pio/esp32dev-serial.log`,
`.pio/quiet-serial.log`. No unresolved Stage 1 architecture ambiguity was found.
Finish-rule selection, session/input role semantics and future feature payloads
were not guessed; they are outside this skeleton's implemented data/behaviour.

### Frozen acceptance result

Tests 1.1 through 1.13 and acceptance-harness sanity test 1.T all passed.
`acceptance/stage1/evidence/results.json` records each frozen criterion, exact
stimulus, expected result, captured actual result, pass/fail outcome, source/spec
commit and build hashes. Raw serial/CLI logs, the deliberate-failure mismatch,
structural review and hashes/source snapshots are retained alongside it.

Repeat the acceptance campaign with:

```powershell
./acceptance/stage1/run.ps1
```

It checks that the frozen acceptance specification has not changed before it
builds and runs the normal and instrumented images. The simulated test fixture
intentionally writes only Wokwi NVS; it never writes credentials or hardware.

## See Stage 1 in the Wokwi browser

In the existing ESP32 custom-firmware project, stop the old plumbing simulation.
Click the editor, press F1, choose **Upload Firmware and Start Simulation…**, and
select `.pio/build/esp32dev/firmware.merged.bin` from this directory. Use the merged
image, not the application-only binary. Automatic browser attachment is still an
unproven tooling step; the automated tests above load firmware through Wokwi CLI.

Watch the **Serial Monitor at 115200 baud** for System Time, Memory, Message Bus
and working configuration READY lines, individual `[TEST] PASS` lines,
`[BUS SELF-TEST] PASS`, and `STAGE1_PASS skeleton initialised; no race behaviour`.
Every ten seconds an IDLE line shows advancing `system_us` and diagnostic drops.

Send `?` for status, `t` to repeat the Message Bus self-test, `q` to silence
diagnostics, or `v` to resume them. These are development controls, not P&P product
Requests or authoritative State. No browser race screen or sensor activity is
expected in Stage 1. Wokwi's Restart button can rebuild the template; use Upload
Firmware again to restart this local image.
