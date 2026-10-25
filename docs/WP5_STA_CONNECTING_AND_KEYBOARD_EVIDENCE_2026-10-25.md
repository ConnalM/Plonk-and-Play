# WP5 STA reconnect and Network-page keyboard evidence — 2026-10-25

## Scope

This is an evidence record for the real-hardware run captured in
`firmware/logs/device-monitor-261025-115541.log`. The persistence retest was
reported PASS separately. No networking or Browser source was changed by this
investigation.

## Observed STA sequence

The controller booted once with the remembered driver-managed STA
configuration. The P&P AP remained available at `192.168.4.1`; SD ownership
and the normal diagnostic boot self-test remained healthy. The capture contains
no Guru Meditation, LoadProhibited, panic, reboot loop, `GOT_IP`, or successful
STA association.

Before the new configuration request the driver repeatedly emitted:

```text
[WIFI] STA_EVENT event=DISCONNECTED reason=15 name=4WAY_HANDSHAKE_TIMEOUT
```

The controlling Browser then produced:

```text
[WIFI] RECONFIGURE_REQUEST route_received=true has_master=true is_master=true owner_connected=true context=RACE_DIRECTOR
[WIFI] RECONFIGURE_ACCEPTED ssid_length=14 password_present=true
[WIFI] STA_BEGIN result=6 status=6
```

The request therefore reached P&P, passed the existing Race Director and
payload validation checks, and invoked the credential-writing `WiFi.begin`
path without exposing credential contents. After that, the driver repeatedly
emitted reason 15 (`4WAY_HANDSHAKE_TIMEOUT`) and reason 8 (`ASSOC_LEAVE`). No
`CONNECTED` or `GOT_IP` event occurred. This proves an association/authentication
failure after the accepted Reconfigure operation; it does not by itself prove
whether the supplied password was wrong, stale, or rejected by the AP's
security/RF conditions.

The STA failure is not an AP failure: the AP stayed available and the Browser
continued to operate through it. The development PC was not routed to the
P&P AP during this capture, so automatic `/diag/download` retrieval was
unavailable from the PC and adds no contrary evidence.

## Why the Browser remains CONNECTING

`firmware/include/pp/external_wifi.h` accepts the asynchronous `WiFi.begin`
result and sets the runtime state to `Connecting`. Its Wi-Fi event callback
only prints disconnect reason/name. It does not update `staFailure_` or the
state. `refreshStaState()` sees the driver's status `WL_DISCONNECTED` (value
6); while `staFailure_` is still `None`, its contract deliberately maps that
combination back to `Connecting`. Consequently the authoritative status
continues to report CONNECTING even after repeated 4-way handshake timeouts,
and the Browser presents the same state.

The smallest later correction is to carry the sanitized station-event failure
into the existing runtime state machine, so a terminal/disconnected driver
outcome is reported as `DISCONNECTED` with the existing bounded failure
reason. That correction is not part of this evidence-only record.

## Separate Browser keyboard defect

The Network page's asynchronous refresh replaces the Network panel DOM every
500 ms while scan/STA connection is considered active
(`firmware/browser/normal_browser.html`, `scheduleNetworkRefresh` and
`networkPage`). Replacing the password `<input>` removes focus and causes a
mobile keyboard to close while the user is typing. The same refresh loop can
make the symptom especially repeatable while STA remains CONNECTING, but it
does not cause the ESP32's reason-15 authentication failure. It is recorded as
a separate Browser UX defect: preserve the active input/focus or avoid replacing
the form while the user is editing it.

## Classification

- Reconfigure request path: **accepted and authoritative**.
- STA outcome: **real driver association/authentication failure**, reason 15,
  with no IP acquisition.
- CONNECTING presentation: **implementation observability/state-mapping defect**.
- Disappearing mobile keyboard: **separate Browser presentation defect**;
  related only in that the active polling loop triggers the DOM replacement.
- Persistence/Results: **separate physical retest PASS**, unaffected by this
  network evidence.
