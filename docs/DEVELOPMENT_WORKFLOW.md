# P&P Development and Test Workflow

**Status:** Proven development baseline  
**Baseline proven:** 30 September 2026

## Purpose

This document records the development/test arrangement used to implement P&P. It is development infrastructure, not part of the P&P product architecture or product contract.

GitHub remains the durable source of truth for accepted specifications and accepted implementation.

## Roles

- **Design/control conversation** — settles product behaviour, architecture and acceptance criteria.
- **GitHub repository** — durable source of truth shared between design and implementation work.
- **Local ChatGPT Work project** — engineering bench: edits the local repository, builds firmware, runs tests, diagnoses implementation failures and makes ordinary implementation decisions within the accepted architecture.
- **PlatformIO** — local ESP32 build/toolchain.
- **Wokwi** — normal virtual ESP32/hardware test bench where suitable.
- **Wokwi Private IoT Gateway** — permits local test clients to reach network services exposed by the simulated ESP32.
- **Automated local clients/browser automation** — exercise HTTP, WebSocket and later browser behaviour.
- **Real hardware** — used at deliberate hardware-validation points for behaviour simulation cannot establish reliably.

Ordinary implementation choices do not require product-design approval. Work should stop and report when implementation exposes a genuine ambiguity or would require changing accepted P&P behaviour, architecture or an agreed interface.

## Proven plumbing test

A temporary project under `experiments/wokwi-plumbing` was used to prove the development path before real P&P firmware implementation.

The following checks passed on 30 September 2026:

1. PlatformIO built the ESP32 firmware successfully.
2. The compiled firmware ran on a simulated ESP32 in Wokwi.
3. A local automated client reached the firmware's `/health` endpoint through the Private IoT Gateway and received HTTP 200 with the expected JSON.
4. A local WebSocket client connected to `/ws` and received an exact echo.
5. Two independent clients connected simultaneously and received the correct distinct responses.
6. One client was disconnected and the other remained operational.

This proves the core automated development path:

```text
local source
    ↓
PlatformIO build
    ↓
compiled ESP32 firmware
    ↓
Wokwi simulated ESP32
    ↓
Private IoT Gateway
    ↓
local automated network clients
    ↓
pass/fail results
```

## What is not yet proven

The plumbing test did not prove:

- fully automatic firmware loading into the Wokwi browser simulator;
- two simultaneously rendered browser clients;
- real detector optics, electrical behaviour, I2C/power integrity, noise or other physical-hardware characteristics.

These are not blockers for Stage 1.

Rendered multi-browser behaviour should be proved when the P&P browser implementation reaches the relevant prototype stage. Real hardware should be introduced when a stage depends on physical behaviour that Wokwi cannot establish.

## Working rules

1. Wokwi and the gateway are test infrastructure only. Production P&P architecture must not depend on them.
2. Work should build, run and test changes itself where practical rather than requiring the user to shuttle code, firmware or compiler output manually.
3. Automated tests should provide clear pass/fail results. Development diagnostics should make failures observable without becoming authoritative product State.
4. Real hardware validation complements simulation; it does not require race/session logic to be redesigned.
5. Accepted changes should be committed to GitHub promptly so implementation does not depend on chat history.
6. Stage work should begin from a clean repository and should not silently alter accepted architecture to make implementation easier.
7. Startup diagnostics must identify the actual build environment accurately. A normal, acceptance, verification, quiet or demonstration image must never present itself as a different stage. These labels are diagnostic only and do not form product State.

## Stage 7 gateway investigation and completed checkpoint

The Stage 7 human Browser checkpoint is **PASSED**. The Stage 7 ESP32 demo was
identified by its `P&P STAGE 7 DEMO` startup diagnostic, and the real rendered
Browser was observed through the Wokwi Private IoT Gateway synchronising,
presenting State and a separate LAP_COMPLETED Fact, progressing to FINISHED,
and reconstructing current FINISHED State after a Ctrl+F5 reload.

The current external-test evidence shows the simulated ESP32 obtained private
gateway address `10.13.37.2`, listens on TCP port 80, receives gateway SYNs and
responds with valid SYN/ACKs. The gateway/Wokwi side did not complete the TCP
handshake. The current official gateway v2.0.1 command and port mapping were
verified. This is being pursued with Wokwi as an external test-environment
defect. P&P networking and BrowserInterface must not be changed to work around
it.

The gateway evidence remains retained as a historical external test-environment
interruption. Automated Stage 7 acceptance remains independent evidence; the
later manual checkpoint supplements it.

## Stage 1 handoff

Stage 1 is governed by the accepted repository specifications, especially `FIRST_IMPLEMENTATION_BEHAVIOUR.md`, `SYSTEM_ARCHITECTURE.md`, `SYSTEM_LIFECYCLE.md` and `MESSAGE_CONTRACT.md`.

The Stage 1 implementation target is the ESP32 skeleton: boot, P&P System Time, Memory access, working configuration in RAM, P&P Message Bus and initial module boundaries, together with the controlled serial diagnostics and Message Bus self-test required by `FIRST_IMPLEMENTATION_BEHAVIOUR.md`.

No race behaviour is required in Stage 1.

## Browser checkpoint reset note

Serial `r` remains the development soft-reset mechanism and preserves the manually uploaded firmware. An in-flight Browser request may show a temporary connection reset while the ESP32 reboots; once the server returns, fresh requests through the existing gateway route should recover. For the Stage 11 demo and acceptance images only, serial `x` performs the fixture TEST RESET and returns the volatile race to clean READY, clearing the retained test Master binding. This is test/demo setup and is not a production race restart or takeover control.


## Stage 11 scenario/stress regression

Generate the permanent deterministic scenario ledger with `python acceptance/stage11/stress_regression.py --seed 11011`. It covers alternating Honour/Grid cycles, unequal progress, awkward Relevant Times and reconnect points. The seed and ordered JSONL ledger are retained with the observed State, Fact and Request Result evidence whenever a real fixture run is attached. This facility supplements the numbered acceptance tests.
