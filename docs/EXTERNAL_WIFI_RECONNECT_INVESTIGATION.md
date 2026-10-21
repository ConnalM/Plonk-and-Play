# External Wi-Fi reconnect investigation

**Investigation date:** 2026-10-21  
**Scope:** morning real-hardware `Disconnect` followed by an attempted return to the remembered house network.  This is an investigation record only; no firmware or Browser behaviour was changed.

## Evidence checked

- `docs/SYSTEM_ARCHITECTURE.md`, external-network rules in the networking section.
- `docs/CONFIGURATION_AND_IDENTITY_SPEC.md`, external Wi-Fi configuration and credential rules.
- `docs/ACCEPTANCE_TESTS_STAGE_15_LOGGING_DIAGNOSTICS_FOUNDATION.md`, NET15.1–NET15.12.
- `firmware/include/pp/external_wifi.h`.
- `firmware/include/pp/browser_interface.h`.
- `firmware/include/pp/normal_browser_page.inc`.
- All locally persisted `firmware/logs/device-monitor-*.log` files.  The newest available log is `device-monitor-261020-215811.log`; no log containing this morning's Disconnect/reconnect interaction is present in the checkout.

## Current implementation

### Disconnect and driver credentials

`ExternalWifiManager::disconnect()` calls `WiFi.setAutoReconnect(false)` and `WiFi.disconnect(false, false)`.  The second argument means the driver's stored STA configuration is retained.  On success the manager sets its runtime suppression flag and reports `DISCONNECTED`; it does not erase the remembered SSID/password.

When suppression is clear, `tick()` calls `refreshStaState()`.  If the driver reports a non-connected state, the manager waits five seconds, increments `reconnectAttempts_`, sets `CONNECTING`, and calls `WiFi.reconnect()`.  This is the driver's stored-configuration path; it does not supply a new password.  A subsequent `WL_NO_SSID_AVAIL`, `WL_CONNECT_FAILED` or `WL_CONNECTION_LOST` is reported as `DISCONNECTED` with the corresponding sanitised failure, and the next five-second retry is deliberate behaviour.  The AP, HTTP service and race authority are independent of this loop.

### What the Browser currently calls CONNECT / RECONFIGURE

The Network page exposes one mutation button labelled `CONNECT / RECONFIGURE`.  Its handler always reads the SSID field and the password field, then sends:

```json
POST /network/connect
{"ssid":"<field>","password":"<field>"}
```

The password input is cleared after submission.  The page does not distinguish a remembered-network reconnect from a credential-changing reconfiguration and does not offer a separate reconnect action.

The route requires the controlling Race Director lease, parses both JSON fields, and calls `ExternalWifiManager::connect(ssid, password)`.  The parser requires the password property to exist but permits it to be an empty string.  `connect()` likewise rejects only a null password pointer or a password longer than 64 characters; an empty password is accepted and passed to `WiFi.begin(ssid, password)`.  That call is the credential-writing/configuration path, not `WiFi.reconnect()`.

The Browser then displays `Connecting…` and polls `/network/status` for up to 15 seconds.  The five-second firmware retry loop continues independently after a failed association.

## Finding

The observed `CONNECTING → DISCONNECTED → CONNECTING` sequence is consistent with the existing intentional retry loop.  The available evidence does not identify whether the radio rejected the network because it was unavailable, because association failed, or because credentials were changed: the morning Serial log and a matching `/network/status` snapshot are not present locally.

The most likely implementation-level cause is the operation chosen in the Browser:

1. `Disconnect` retained the driver's remembered credentials and suppressed reconnect as designed.
2. The only visible way back was `CONNECT / RECONFIGURE`.
3. With no password re-entered, the Browser sent an empty password.
4. P&P accepted that request and called `WiFi.begin(ssid, "")`, which can replace the driver's stored secured-network password with an empty password.
5. The subsequent association failed, while `stationConfigured_` remained true, so the normal five-second retry loop repeatedly attempted the now-invalid driver configuration.

This is a likely root cause from the current code, but it is not a fully proven hardware cause without the missing run evidence.  The fact that the SSID remains shown as remembered does not prove the password remained intact: the runtime flag is independent of the driver's password contents.

## Product/design boundary

The frozen documents define remembered credentials, Disconnect suppression, and Connect/Reconfigure, but do not define a distinct user operation for “reconnect using the remembered driver configuration without entering credentials.”  The current UI therefore conflates two different intents:

- **Reconnect:** clear Disconnect suppression and use the already remembered driver configuration.
- **Reconfigure:** submit a new SSID/password and replace the driver configuration.

The proposed distinction in the investigation request is a product/UX clarification and has not been treated as authorised implementation.  Archie/Connal should decide the exact labels, visibility and request contract before code changes.  The decision must preserve the existing sole driver-managed credential store and must not expose or duplicate the password.

## Browser sluggishness

No current log contains the morning race or request timings, so there is no evidence that STA retry work caused the reported sluggishness.  The implementation performs association/retry calls from the non-blocking network manager and keeps AP/HTTP/race paths separate.  The observation remains uncorrelated until a matching Serial/diagnostic run is available.

## Status

- **Likely root cause:** CONNECT / RECONFIGURE is being used as a password-less reconnect and may overwrite the remembered secured-network password with an empty password.
- **Retry sequence:** deliberate current retry behaviour, with sanitised failure state; not by itself proof of a new defect.
- **Credential retention:** Disconnect retains credentials; after the attempted blank-password Connect, retention of the original password is not guaranteed by the current implementation.
- **Product decision required:** define and authorise the separate remembered-network reconnect versus credential-changing reconfigure workflow.
- **Implementation:** none in this investigation.
