# Framework migration first hardware smoke — 2026-10-26

Candidate: `a9636c43ab89cca6ab4bb9e656c143b1e0e27e76`, environment `stage8bp1s3sd`; host application image SHA-256 `B3423B0517917242653AB0650108DEEB5C2DEA4C53E872B4DBBE8955D727A7F9`, ELF SHA-256 `13C87A662B800906130179C8306E70401B28AB25EDF9BCF1080D319C38B655A2`. No race was run and no firmware, NVS or SD contents were deliberately modified after upload.

## Capture and provenance

COM7 Serial capture was confirmed live after upload. The first capture did not survive the requested physical power-cycle because the USB serial device disappeared; that boot's startup evidence was missed. A reconnecting monitor was established, then a serial-line reset was used with capture attached. The captured boot reports commit `a9636c43ab89cca6ab4bb9e656c143b1e0e27e76` and the correct environment. Both Serial and `/health` report application ELF SHA prefix `13c87a662`, matching the host ELF; they do not expose the full hash in this run. The new SDK configuration contains `CONFIG_APP_RETRIEVE_LEN_ELF_SHA=9`; the cause of the shortened reported string has not been established by this observation alone. The capture is `firmware/logs/device-monitor-migration-261026-142430.log` in the migration worktree (local, not committed). After the captured reset no panic or unexpected reboot was observed during the smoke period; periodic output reported `dropped=0`.

## Observations

- SD: first captured boot attempt reported `SD owner mount OK`; 15,931,539,456 card bytes, 15,926,558,720 filesystem bytes and 22,282,240 used. History boundary reported available with lazy reconstruction.
- Existing records: `/records` returned persisted lane PBs 1,977,943 and 3,400,239 microseconds and Track Record 1,977,943 microseconds. The stored History smoke check did **not** pass: `/history` returned the incomplete body `{"entries":[` and terminated early; curl reported error 18 and Serial reported `HTTP ERROR route=/history duration_ms=36 result=260`. The route uses 260 (`ESP_ERR_INVALID_SIZE`) for more than one underlying History load/validation/serialization failure. The exact cause is not established. Do not infer SD data loss from this observation.
- Network: AP started at 192.168.4.1, with one client. The remembered STA configuration was detected, associated, and obtained LAN address 192.168.68.102 without credentials being re-entered; `/network/status` showed configured and CONNECTED, lastFailure NONE and reconnectAttempts 0. mDNS startup was reported.
- HTTP: `/health` succeeded and `/` returned HTTP 200, 60,170 bytes, in 0.221 seconds for the recorded LAN request. Serial separately logged `HTTP ERROR route=/normal duration_ms=156 result=45062` from an unattributed request; the successful root fetch does not explain or negate that error.
- Display: firmware logged display init OK and READY FOR WORK, but Connal reported that the visible display was not as expected. The exact visual symptom and cause were not established. This is not a display physical PASS.
- Idle resource snapshot from the existing serial health command: free heap 197,564 bytes, minimum heap 190,160 bytes, largest allocation 155,636 bytes; `stack_hwm_words=4324` as labelled in the firmware. This is a single sample, not a worst-case or per-task qualification. No performance/race test was performed.

## Outcome

Migration smoke is **not a full PASS**: display appearance failed the operator check and previously stored History could not be read successfully. Boot identity, UART, first SD mount, persisted Track Record read, AP, remembered STA, one LAN root fetch and `/health` were observed as above. Preserve these failures separately; neither an HTTP redesign nor a race/persistence behavioural change is authorised by this smoke result. Stop for Archie review before further physical testing.
