# WP5 STA state and Network form correction — 2026-10-25

## Implemented corrections

The existing AP+STA and credential authority model is unchanged.

1. `ExternalWifiManager` now records a sanitized terminal station failure when
   the ESP32 driver emits a disconnect while an association attempt is in
   progress. The runtime state becomes `DISCONNECTED` with `CONNECT_FAILED`;
   the bounded reconnect tick may later begin a new attempt. Deliberate
   Disconnect/Forget suppression remains `DISCONNECTED` with no failure.
2. The Normal Browser no longer replaces the Network form while the SSID or
   password input is the active element. Polling continues, but the form is
   left intact until editing ends. Explicit Scan, Connect, Reconnect,
   Disconnect and Forget actions force their authoritative redraws.

The existing Stage 15 network contract regression now covers both behaviours,
including a terminal disconnect model and the active-form/forced-action model.

## WPA association evidence and boundary

The latest automatic capture is
`firmware/logs/device-monitor-261025-115541.log`.

The accepted Reconfigure request was authoritative and contained the JSON
password property, but the sanitized `password_present=true` log only proves
property presence; it does not prove that the value was non-empty or correct.
The driver then reported repeated reason 15 (`4WAY_HANDSHAKE_TIMEOUT`) and
reason 8 (`ASSOC_LEAVE`), with no `CONNECTED` or `GOT_IP`. This is a real
association/authentication failure, while the P&P AP remained available.

The Arduino-ESP32 `WiFi.begin(ssid,password)` path calls the ESP-IDF config
and connect sequence and returns an immediate `wl_status_t`; that immediate
value is not the eventual association result. Its `wifi_sta_config` helper
sets the secured threshold and password only when the supplied password is
non-empty. `WiFi.persistent(true)` therefore makes an accepted Reconfigure
the driver-managed credential-writing path, as designed.

The same sequence has previously succeeded on this controller: earlier logs
show accepted Reconfigure followed by `CONNECTED` and `GOT_IP`, and also show
that an immediate `STA_BEGIN` result of `WL_CONNECT_FAILED` can precede a later
successful association. Therefore the current evidence does not justify
classifying the latest failure as a P&P sequencing defect or simply as a bad
password. The disappearing keyboard could have affected entry, but the
available evidence cannot establish that.

Before selecting a #1 association correction, the smallest safe next evidence
step is a sanitized password-length observation at the accepted route together
with the existing driver reason/association events. No password value or
credential fingerprint is required. That distinguishes an empty/truncated
Browser submission from a genuine driver/AP authentication failure without
changing credential semantics or adding a second store.

## Scope and next review boundary

The terminal-state and active-form corrections are software-only and ready for
review. The WPA association failure remains a separate open hardware/network
investigation. No physical retest is requested by this change.
