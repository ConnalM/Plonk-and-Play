# Human Acceptance Checkpoint Procedure

**Status:** Proposed — for review before use

## Purpose

This procedure makes human acceptance checkpoints repeatable. Environment
preparation is separate from the checkpoint itself: Connal begins the
checkpoint only after the coding agent has established a known working state.

Individual frozen acceptance plans remain authoritative for their required
behaviour, fixture, firmware environment, and expected observations. This
procedure defines how to prepare and conduct their human portion.

## 1. Agent preparation before involving Connal

Before asking Connal to perform a manual action, the coding agent must:

1. Build the exact firmware environment required by the checkpoint.
2. Verify that the required deterministic demonstration fixture,
   configuration, and controlled sources exist in that environment.
3. Confirm automated acceptance work is complete enough for the human
   checkpoint to begin, including required regression and structural checks.
4. Define the complete manual sequence in advance. For every step, record the
   action, the expected visible result, and the point at which any Browser
   reconstruction or refresh check is required.
5. Dry-review that sequence from a fresh local restart. It must establish its
   required initial State after local simulator, gateway, Browser, race, and
   session processes or State have been restarted; it must not depend on useful
   State surviving from an earlier run. Confirm that it can reach every State
   required by the frozen plan without adding acceptance-only shortcuts or
   changing normal product defaults.
6. Provide Connal with the exact firmware path, its expected `[DEV]` startup
   identity, and whether the checkpoint needs Wokwi, the Private IoT Gateway,
   and/or the diagnostic Browser.

The agent must not use the live human checkpoint to discover the intended
sequence, diagnose an unfinished fixture, or decide which State should be
observed next.

A status update is not a stopping point. During authorised preparation and
checkpoint support, the agent continues through diagnostics, non-destructive
repairs, builds and verification without waiting for further prompting. It asks
Connal only for a physical/manual action it cannot perform, an approval that is
actually required, a genuine unresolved design decision, the prepared human
checkpoint, or final completion.

## 2. Establish the known initial state

This setup is not part of the human acceptance checkpoint. Complete it before
asking Connal to start the defined manual sequence.

1. Start or open the required Wokwi project.
2. Use Wokwi's **Upload Firmware and Start Simulation** command to upload the
   firmware specified by the individual acceptance plan.
3. Confirm the Serial Monitor shows the exact expected `[DEV]` identity.
   Treat a different identity as setup failure, not product evidence.
4. If the checkpoint requires the Private IoT Gateway, start or restart it
   before Browser use. The standard local command is:

   ```powershell
   & "C:\Users\conna\.wokwi\gateway-v2.0.1\wokwigw.exe" --forward 9080:10.13.37.2:80
   ```

5. The coding agent performs technical checks of the gateway process,
   forwarding route, local listener, and service reachability wherever it can.
   Do not ask Connal to verify ports, routes, processes, or other technical
   conditions. If a required check genuinely needs Connal, give exactly one
   concrete action and state the exact visible result he should report.
6. If the checkpoint requires Browser presentation, open or refresh the
   diagnostic Browser at `http://127.0.0.1:9080/`.
7. Verify the exact expected initial Browser State and that it displays
   `synchronised`.
8. Only now state that the human acceptance checkpoint has begun.

The individual acceptance plan supplies the stage-specific firmware path,
expected serial identity, initial State, and any non-default gateway mapping.
This procedure deliberately does not hard-code them.

## 3. Conducting the checkpoint

- Give Connal exactly one simple manual action at a time.
- State what should be visible after that action, using the predetermined
  sequence.
- Wait for Connal's observation before proceeding.
- Compare the observation with the predetermined expected result after every
  action.
- Perform refresh, reload, reconstruction, or reconnection checks only at the
  point defined in advance.
- Do not redesign the test, add exploratory steps, or create new acceptance
  criteria while the checkpoint is running.

If setup is wrong or the environment loses its known state, stop the
checkpoint. Repair the environment, restore the defined initial state, and
restart the human checkpoint cleanly. Record this as a setup/environment
failure; do not treat it as a product acceptance failure unless the frozen
plan makes the failed condition a product requirement.

## 4. Completion and evidence

When the predetermined final observation is reached:

1. State the human checkpoint result explicitly as **PASS** or **FAIL**.
2. Retain concise evidence covering the fixture identity, relevant manual
   actions, expected and observed results, and final authoritative State.
3. Link that evidence from the stage's acceptance evaluation or evidence
   manifest.
4. Do not ask Connal to perform further manual actions unless the frozen plan
   requires them.

A successful human checkpoint supplements automated evidence. It does not
replace frozen automated acceptance tests, structural checks, or required
regressions.

