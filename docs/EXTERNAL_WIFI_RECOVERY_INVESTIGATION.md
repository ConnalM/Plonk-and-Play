# External Wi-Fi recovery investigation

**Investigation date:** 2026-10-21  
**Firmware under test:** `5ff85e5b66c3d7e49fe8f7490769a3952f4f8bac`  
**Scope:** remembered external Wi-Fi reached `DISCONNECTED`, followed by an attempted scan and `RECONFIGURE`. Investigation only; no firmware or Browser change was made.

## Evidence checked

The newest persisted serial capture is `firmware/logs/device-monitor-261021-125221.log`. It contains a normal boot, the factory AP, mDNS, one AP client and the line `external STA remembered configuration detected`. It does not contain a scan result, an STA failure reason, a `/context` snapshot or a Browser identity. It does contain three later entries:

```text
[DEV] HTTP ERROR route=/network/connect duration_ms=9 result=258 ...
```

The ESP-IDF value `258` is `ESP_ERR_INVALID_ARG`. In this route that value is assigned by the request-body parser failure path; the controlling-lease rejection path assigns `ESP_ERR_INVALID_STATE` (`259`). The log therefore shows that the reconfigure requests reached the P&P route and failed validation before `ExternalWifiManager::connect()` was called. It does not show a credential write in this run.

The capture contains no `Guru Meditation`, reset header, STA password, SSID password material, scan result or explicit `/network/scan` outcome.

## Reconfigure path

The Network page sends `POST /network/connect` with JSON containing the SSID and password fields. The route first checks the current controlling Race Director lease, then requires a valid non-empty SSID and a password property, and only then calls `ExternalWifiManager::connect()`.

The current log's `result=258` identifies the body-validation branch, rather than an authority rejection (`403`, trace result `259`) or a remembered-network association failure (`409` after `connect()` returns false). The observed “Connection request rejected” is therefore not evidence that a remembered-but-disconnected state is forbidden. It is an invalid request reaching the route. The exact malformed/missing body is not recoverable from the existing sanitized log because request bodies are deliberately not recorded.

The approved workflow does permit recovery from invalid remembered credentials: the controlling SMUG uses explicit **RECONFIGURE** with a selected SSID and newly entered password. **RECONNECT** is the separate no-credential operation for a remembered configuration. A failed remembered association does not by itself prohibit reconfiguration. A non-controlling Browser must still be rejected by the lease check, but that is not the branch evidenced by this capture.

The current request did not reach `WiFi.begin()` and therefore did not overwrite the driver-managed credential store during the recorded attempts. Whether the older blank-password incident had already damaged the stored password cannot be determined from this run, by design: password material is never exposed. The boot line proves that a stored STA SSID/configuration was detected, not that its password is valid.

## Scan path

Scan is observational and has no Race Director check. The Browser sends `POST /network/scan`; P&P calls `WiFi.scanNetworks(true, true)` and returns `202` when the asynchronous scan is accepted or already running, otherwise `409 SCAN_UNAVAILABLE`. The Browser then polls `/network/status` while `scan.running` is true and renders the returned network list.

The current capture has no `/network/scan` trace, and successful scan routes are not logged with their accepted/failed result. Consequently the evidence cannot distinguish “the click/request did not leave the Browser” from “the driver rejected/failed the asynchronous scan” or “the scan completed with zero results.” The source contains no authority or remembered-credential condition that should intentionally disable scanning. If a scan was attempted while the STA retry loop was actively reconnecting, the Arduino/ESP32 driver may return `WIFI_SCAN_FAILED`; that is a state-dependent driver outcome to verify with a route/status trace, not a product rule.

The Browser does have a visible scanning state and bounded polling. The current implementation therefore has an evidence/diagnostic gap for scan failures, but the existing frozen design already makes the intended behaviour clear: any connected Browser may request a sanitized scan and selecting an SSID is local presentation state.

## Common cause and boundary

The two symptoms do not share one proven backend cause:

* Reconfigure is proven to have reached `/network/connect` and failed request-body validation. Authority is a separate preceding branch; no authority failure is evidenced here.
* Scan is authority-independent, but its outcome is absent from the serial evidence. A busy/retrying STA driver is a plausible state-dependent cause, while a Browser/request omission remains possible because the current firmware does not trace the route outcome.

The remembered STA retry loop is deliberate: `ExternalWifiManager::tick()` retries with `WiFi.reconnect()` after its bounded interval and reports `CONNECTING`/`DISCONNECTED` without blocking the AP, HTTP service or racing. The capture does not expose the sanitized failure state or retry count for this run.

No new product or design decision is required. The frozen AP+STA and Network contracts already define the recovery operation and scan authority. A follow-up implementation investigation should add only enough sanitized route/status observability to identify scan acceptance/completion/failure and the reconfigure validation reason without recording credentials; it should then address the concrete request/UI or driver-state defect found by that evidence.

## Conclusion

* **Likely reconfigure root cause:** the submitted request was rejected by `networkCredentialsFromBody()` (`ESP_ERR_INVALID_ARG`), before credential writing. It was not a remembered-state prohibition and not the controlling-lease branch in the available evidence.
* **Scan root cause:** not conclusively identifiable from the available capture because `/network/scan` outcomes are not logged and no status snapshot was retained. The source permits scan for all connected Browsers; a driver failure while STA retry is active is plausible but unproven.
* **Credential state:** the run detected a remembered driver configuration. The old password's validity is intentionally unknowable from sanitized evidence; this run did not write a replacement password.
* **Product decision:** none required. The approved workflow is sufficient; this is an implementation/evidence follow-up, not a scope change.

## Follow-up implementation

The approved correction preserves Network-page drafts while asynchronous STA or
scan polling redraws the page. SSID/password values are captured from the
controls before refresh, restored only in Browser memory, and submitted as the
explicit JSON `{ssid,password}` payload. The password is cleared after
submission and is never sent to diagnostics or Serial. The P&P parser now
terminates the received body explicitly and returns sanitized validation detail
for malformed requests.

Scan now has a bounded P&P-owned lifecycle (`IDLE`, `RUNNING`, `COMPLETE` or
`UNAVAILABLE`) with request, acceptance, completion and failure counters and a
numeric driver result. `/network/scan`, `/network/status` and the normal Serial
health output expose only this non-secret state. The Browser keeps polling for
the bounded scan window even when the first status sample still predates the
driver's asynchronous `RUNNING` transition. No authority, credential-store,
AP+STA or race behaviour changed.

The implementation was checked by `acceptance/stage15/run_network_contract.ps1`,
the Stage 14C Browser polling/recovery/presentation tests, the Stage 15 control
lease and SSE tests, and the WP3 Browser/export/production tests. The
`stage8bp1s3sd` build succeeded. The merged image is a development artifact at
`firmware/.pio/build/stage8bp1s3sd/firmware.merged.bin` (1,089,392 bytes,
SHA-256 `871EB19F944108A3F9432DF2EED075181AA0EEA4FD18B8E93C786EAE26C8CA3B`),
with the standard ESP32-S3 offsets. Real-hardware reconfigure and scan
acceptance remains to be exercised.

## 587ea41 clean-root real-hardware checkpoint

**Investigation date:** 2026-10-21  
**Firmware under test:** `587ea41ea1d5b8890c4a63a3ad903a9408c80e3d`  
**Evidence:** `firmware/logs/device-monitor-261021-212710.log`  
**Scope:** current Normal Browser at `/`; one Scan request sequence and one
manual Reconfigure sequence from the controlling Browser. The capture was
read in full, including the boot, periodic health output and request markers.

### Run integrity and independent storage observation

The application has one ROM boot header (`rst:0x1 (POWERON)`) and no
`Guru Meditation`, `panic`, `EXCCAUSE`, `EXCVADDR` or second reset header. The
factory AP and HTTP server started, the AP had one client, and the periodic
boot-health line continued with `dropped=0`. The physical SD owner failed its
mount attempt and the diagnostic backend reported its existing degraded
storage state. That is the already recorded SD/storage issue; it is separate
from the Wi-Fi request outcomes and did not prevent the AP or HTTP service
from running.

### Scan: demonstrated driver-state failure

The three scan request records in this capture are:

```text
[WIFI] SCAN_REQUEST route_received=true
[WIFI] SCAN_REQUEST received count=1
[WIFI] SCAN_START_PREP mode=3 mode_result=0 sta_status=6 scan_before=-2 ap_started=true
[WIFI] esp_wifi_scan_start result=12294 name=ESP_ERR_WIFI_STATE block=false channel=0 scan_type=0
[WIFI] SCAN_UNAVAILABLE result=-2 failures=1
```

The same sequence repeats for counts 2 and 3. `mode=3` is AP+STA,
`sta_status=6` is the Arduino `WL_DISCONNECTED` state, and `scan_before=-2`
shows that no earlier scan was running. The ESP-IDF wrapper reports
`12294` (`ESP_ERR_WIFI_STATE`), which Arduino exposes as `WIFI_SCAN_FAILED`
(`-2`). The route therefore returned the authoritative unavailable/rejected
outcome; this was not a missing Browser request, a lease gate, or a completed
zero-result scan.

The remembered STA was in its normal bounded reconnect lifecycle at the same
time: the boot had detected driver-managed configuration and the manager was
trying to associate while the status remained disconnected. The concrete
failure is therefore a scan start attempted while the ESP Wi-Fi driver was in
an incompatible STA/AP state. The capture does not provide an ESP-IDF
disconnect reason code, authentication result, or association reason for the
house network; those values are not emitted by the current instrumentation.

The smallest implementation correction to review is to coordinate the
observation-only scan with the STA reconnect lifecycle: do not call
`esp_wifi_scan_start` while a STA connect/reconnect operation is in flight;
hold the bounded scan request until the driver is in a scan-eligible state,
then resume the existing bounded reconnect behaviour. This preserves the
existing AP+STA and non-blocking contracts. It should be implemented only
after the normal driver-state coordination path is reviewed; no speculative
credential or product change is justified by this capture.

### Reconfigure: request accepted and association outcome unavailable

The capture contains two request sequences (the server received two
credential submissions during the logged run):

```text
[WIFI] RECONFIGURE_REQUEST route_received=true has_master=true is_master=true owner_connected=true context=RACE_DIRECTOR
[WIFI] RECONFIGURE_ACCEPTED ssid_length=13 password_present=true
```

The same sequence appears again. This proves that the current Normal Browser
sent the request, the authoritative controlling lease was valid, the SSID
passed validation, and `ExternalWifiManager::connect()` was invoked. The
implementation has `WiFi.persistent(true)` and calls `WiFi.begin(ssid,
password)` on this path; the driver-managed configuration is therefore the
only credential store being asked to retain the supplied configuration. No
password value is present in the log.

After acceptance, the capture contains no ESP-IDF station event, driver
disconnect reason, DHCP result, or `HTTP HEALTH` status snapshot. The
Browser's observed `CONNECTING` then `DISCONNECTED` is consistent with the
authoritative asynchronous state, but the available evidence cannot classify
the association failure as authentication, missing SSID, timeout, or another
driver reason. The code currently discards the `WiFi.begin()` return value and
does not log the subsequent station event/reason, so no narrower root cause
can be claimed without changing instrumentation or inspecting the driver
event stream.

### Relationship and disposition

The two failures are not one common request/authority defect. Scan reached
the route and failed synchronously in the ESP-IDF driver with
`ESP_ERR_WIFI_STATE`. Reconfigure reached the route and was accepted by the
authority/parser boundary; its later association failure is not diagnosed by
this capture. Both occur while the same remembered STA is disconnected and
retrying, so the STA lifecycle is a plausible shared environmental condition,
but only the scan-side state conflict is demonstrated.

No crash, reset, storage error beyond the pre-existing SD mount failure, or
credential leakage was observed. The HTTP error/slow lines concern Browser
page delivery/connection churn and do not establish a Wi-Fi association cause.
No product decision is required. A firmware correction is required for scan
eligibility coordination; a separate sanitized station-event diagnostic is
needed before selecting a precise Reconfigure/association fix.

## 107d0fb real-hardware checkpoint

**Investigation date:** 2026-10-21
**Firmware under test:** `107d0fb17ae3a0f3d6aa6f6b3215202b28c7c241`
**Evidence:** `firmware/logs/device-monitor-261021-153607.log`
**Scope:** one boot followed by one Scan and one attempted Reconfigure; no source change was made.

The capture contains one boot/reset header only (`rst:0x1 POWERON`), a successful
PhysicalSdOwner mount, the factory AP and mDNS startup, and
`external STA remembered configuration detected`. It contains no panic,
`Guru Meditation`, `LoadProhibited`, or second reboot. The AP remained available
and the periodic health line continued with `dropped=0`.

### Scan

The Scan request definitely reached P&P. The relevant serial sequence is:

```text
[WIFI] SCAN_REQUEST route_received=true
[WIFI] SCAN_REQUEST received count=1
[WIFI] SCAN_UNAVAILABLE result=-2 failures=1
```

`startScan()` calls `WiFi.scanNetworks(true, true)` and treats the Arduino
ESP32 `-2` result (`WIFI_SCAN_FAILED`) as unavailable. The route therefore
returned the existing `409 SCAN_UNAVAILABLE` response; the Browser's “Scan
unavailable” message accurately represented the authoritative outcome. This was
not a missing route request or a zero-result completed scan. The available
evidence does not identify the lower-level reason why the ESP32 scan failed
(for example, transient STA/AP driver state), so it does not justify a source
change or a product decision by itself.

### Reconfigure

There is no `RECONFIGURE_ACCEPTED`, `RECONFIGURE_REJECTED`,
`/network/connect` HTTP error, or other connect-route marker anywhere in this
capture. The request therefore did **not** reach P&P's `/network/connect`
handler. No credential parser ran, `ExternalWifiManager::connect()` was not
called, and the driver-managed credential store was not changed by this
attempt. The boot evidence only confirms that a remembered driver configuration
was present; password validity remains intentionally undisclosed.

The current Browser renders the mutation controls disabled unless the
authoritative `/context` response says `isMaster=true`:

```js
const control=!!C?.isMaster;
... id="networkConnect" ${control?'':'disabled'} ...
```

Scan is observational and has no such gate. Thus the combination of a working
Scan request and a Reconfigure press with no server trace is consistent with the
Browser not recognising a controlling lease (or the control button being
disabled before the click). The Serial capture does not include `/context`, so
it cannot distinguish that precise UI state from a click that never generated a
request for another client-side reason. It does establish that this was not a
P&P request validation or Wi-Fi association rejection.

### Common-cause assessment

The two observations are not one proven backend failure:

* Scan reached P&P and the Wi-Fi driver returned `WIFI_SCAN_FAILED` (`-2`).
* Reconfigure did not reach P&P at all; no SSID/password was accepted or
  written.

The development PC was not on a route to the controller during this capture:
`192.168.4.1` was unreachable and `plonk-and-play.local` did not resolve from
that host, so no live `/network/status`, `/context`, or `/diag/download` query
could be added. No product/design decision is required by this evidence. A
future targeted capture should include the Browser's authoritative `/context`
state at the time of the Reconfigure press and, if needed, the scan status
response; it should not record credentials.

## eef9a83 continuation: repeated Reconfigure and Scan checkpoint

**Investigation date:** 2026-10-21  
**Firmware under test:** `eef9a83da0ee1e71d4e6d284870cd8fd6efbac80`  
**Evidence:** `firmware/logs/device-monitor-261021-170305.log`  
**Scope:** remembered STA repeatedly reported by the operator as
`CONNECTING` → `DISCONNECTED`; two Scan presses and one Reconfigure press were
made from the Network page. No source change was made.

### Reconfigure request path

The capture contains no `/network/connect`, `RECONFIGURE_ACCEPTED` or
`RECONFIGURE_REJECTED` marker. Therefore the Reconfigure press did not reach
P&P's connect route: the credential parser did not run, `ExternalWifiManager::connect()`
was not called, and the driver-managed credential store was not touched by
this press.

The current Browser implementation makes the no-request mechanism explicit.
`networkPage()` computes `const control=!!C?.isMaster` and emits
`#networkConnect` disabled unless that authoritative context value is true.
The click handler also returns immediately when the button is disabled, before
constructing `fetch('/network/connect', ...)`. `C` is populated only by a
successful `/context` response during the authoritative poll; Browser code has
no local authority fallback.

This run contains one `/context` HTTP error:

```text
[DEV] HTTP ERROR route=/context duration_ms=5 result=45062 ...
```

`45062` is `0xB006`, `ESP_ERR_HTTPD_RESP_SEND`, meaning the HTTP server could
not send that response packet to the client. The capture therefore does not
contain the `/context` body and cannot distinguish whether the controller's
authoritative lease was absent, or whether a valid lease response was lost at
the Browser/transport boundary. It does establish that the observed silent
press was client-side gating/no request, rather than a P&P validation,
authority rejection (`403`), or Wi-Fi association rejection. A request that
did reach the route would have produced a `/network/connect` trace even when
the lease check rejected it.

The frozen NET15.3 rule remains satisfied by this boundary: only a recognized
controlling Browser may mutate network configuration. The evidence instead
identifies a Browser/transport observability and recovery gap when authoritative
`/context` is unavailable; it does not justify weakening the lease check.

### Scan request path

Both Scan presses reached P&P and failed identically:

```text
[WIFI] SCAN_REQUEST route_received=true
[WIFI] SCAN_REQUEST received count=1
[WIFI] SCAN_UNAVAILABLE result=-2 failures=1
[WIFI] SCAN_REQUEST route_received=true
[WIFI] SCAN_REQUEST received count=2
[WIFI] SCAN_UNAVAILABLE result=-2 failures=2
```

`ExternalWifiManager::startScan()` calls Arduino-ESP32
`WiFi.scanNetworks(true, true)`. In the actual framework used by the S3 build,
that function returns `WIFI_SCAN_FAILED` (`-2`) when the underlying
`esp_wifi_scan_start()` call is not accepted; the route consequently returns
the authoritative `409 SCAN_UNAVAILABLE`. This is not a missing Browser route,
an empty completed result, or a control-lease gate. The source and Serial
evidence do not expose the lower-level ESP-IDF rejection reason. The failures
occurred while the remembered STA was repeatedly trying to associate in
AP+STA mode, which is a plausible driver-state interaction but is not proof of
the precise lower-level cause.

### Relationship and disposition

The two symptoms are not one demonstrated backend fault:

* Reconfigure: no request was emitted because the Browser did not have a
  successful authoritative `isMaster` context at the time of the press; the
  exact lease-versus-response-loss branch remains unproven.
* Scan: the request arrived and the ESP32 Wi-Fi driver rejected scan start.

The frozen product behaviour requires both a usable observational Scan and a
controlling-SMUG Reconfigure path. A firmware/driver-level Scan correction is
therefore still required, but its exact implementation cannot be chosen from
this capture alone. A Browser/transport correction is required if a valid
controlling lease is confirmed while `/context` is temporarily unavailable;
the next evidence must capture a successful `/context` body or a deliberate
control request before selecting that correction. No product/design decision is
required by the current evidence, and no credentials were logged or exposed.

## Targeted fault-finding instrumentation (ready for one hardware run)

The next disposable Phase 8B image adds credential-free tracing only; it does
not change lease, scan, credential, or network behaviour. The `/context`
response trace reports `has_master`, the requesting client's `is_master`,
controller-owner connectivity, takeover state, the authoritative result, and
HTTP send status when that tuple changes or a send fails. The Reconfigure route
logs receipt with the same lease facts, then its existing sanitized validation/
acceptance outcome. The Browser emits bounded observations when its Network
panel's authoritative control state changes and when Reconfigure is clicked,
including whether the rendered button was enabled. The ESP-IDF linker wrapper
logs the exact `esp_wifi_scan_start` result/name and non-secret scan parameters
while delegating immediately to the real driver call; the existing scan route
and result handling remain unchanged. Scan preparation also records Wi-Fi mode,
mode-query result, STA status, prior scan state, and AP availability.

The minimum hardware sequence is: flash this image; connect one Browser to the
P&P AP; acquire Race Director control if required; open Advanced → Diagnostics
→ Network; capture one Network-page load, one Scan press, and one Reconfigure
press with the currently remembered SSID/password state; then stop. The full
Serial capture must be inspected for the new `[AUTH]`, `BROWSER_OBSERVATION`,
`RECONFIGURE_REQUEST`, and `esp_wifi_scan_start` markers together with panic,
reset, SD/FAT, HTTP, and transport errors.

## 2b750a9 complete instrumented hardware run

**Investigation date:** 2026-10-21  
**Firmware under test:** `2b750a9e36ccaa2c36850e00722bc936ceffd5b6`  
**Evidence:** `firmware/logs/device-monitor-261021-175016.log` (1,250 lines)

The capture contains two boot segments. The first segment runs to roughly
11.6 billion microseconds; a second explicit ROM header then reports
`rst:0x1 (POWERON)` and starts another clean application boot. The log does
not contain a panic or a reason for that reset, so the reset is retained as an
unexplained capture-boundary/reboot observation rather than silently counted
as a single uninterrupted run. Each segment shows the physical storage owner
mounting once, the factory AP starting at `192.168.4.1`, mDNS starting, and the
HTTP server starting with `error=0`. There are
no `Guru Meditation`, `panic`, `EXCCAUSE`, `EXCVADDR`, HTTP error/slow, or Wi-Fi
scan/association crash markers. The repeated
`records.roll.tmp does not exists or is directory` messages remain the separate
known cleanup issue and are not evidence of a crash in this run.

### A — false “P&P AP unavailable”

The AP-unavailable presentation is a Browser-path/presentation defect, not an
AP failure. The same capture proves the AP was running and had the tablet as a
client. It also contains two successful authoritative context responses:

```text
[AUTH] CONTEXT response has_master=true is_master=true owner_connected=false takeover_pending=false requested_by_this=false result=NONE sent=0
[AUTH] CONTEXT response has_master=true is_master=true owner_connected=true takeover_pending=false requested_by_this=false result=NONE sent=0
```

Thus the tablet's control lease and HTTP context path were working. The current
Normal Browser source renders AP availability from `/network/status.ap.started`,
and the generated `/normal` artifact in this commit contains that path. The
legacy root route `/` still serves the older diagnostic Browser and does not
serve the current Network page. The run contains none of the new
`BROWSER_OBSERVATION`, `/network/scan`, or `/network/connect` markers, so the
page that produced the observed Network presentation did not execute the
instrumented current Network-page path. The concrete correction is to make the
customer entry point unambiguously serve the current Normal Browser (or otherwise
guarantee that the tablet uses `/normal`) and to remove the parallel stale
presentation path from the shipped workflow. No AP or authority correction is
indicated by this evidence.

### B — Scan failure

This run does not establish the low-level cause of the Scan failure. There is no
`[WIFI] BROWSER_OBSERVATION`, `SCAN_REQUEST`, `SCAN_START_PREP`, or wrapped
`esp_wifi_scan_start` line. Therefore no scan request reached the instrumented
route in this capture, and the driver result cannot be attributed to a particular
ESP-IDF mode/state error. Earlier runs established that the route can return
`WIFI_SCAN_FAILED (-2)`, but that is not newly explained here.

The concrete next correction is the Browser-entry/path correction above, followed
by making the scan request/result path observable on the actual served page. Only
then can the existing raw-driver instrumentation distinguish a real ESP-IDF
rejection from another stale-page/request problem. No product decision is needed;
the missing fact is a driver-level scan call, which this run did not make.

### C — Reconfigure did nothing

The absence of both `BROWSER_OBSERVATION kind=network_reconfigure_click` and
`RECONFIGURE_REQUEST` proves that the click never reached the current Browser
handler or P&P's `/network/connect` route. It was not rejected by the Race
Director lease, credential parser, or Wi-Fi manager. The `/context` evidence
shows `is_master=true` for the tablet, so authority was available. The current
Normal Browser handler records the click before its disabled-button guard and
then constructs `/network/connect`; neither side appeared in the capture.

The smallest correction is therefore the same entry-path correction as A: ensure
the physical customer Browser is the current generated Normal Browser with the
instrumented Network handlers, rather than allowing the legacy root Browser or a
stale cached artifact to be used for product networking. No lease weakening or
credential-path change is justified.

### Shared cause and remaining evidence

A and C share the demonstrated Browser-surface/path problem. B is not shown to
share the Wi-Fi driver failure because this run did not invoke the instrumented
scan endpoint at all. Apart from the explicit ROM reboot header described above,
the capture contains no HTTP transport errors, SD/FAT mount failure,
dropped-event report, or other new cause for the
sluggish presentation. The dense periodic boot-self-test output and repeated
temporary-file removal errors are observability noise and a separate cleanup
follow-up; they do not establish a Race Engine or network timing fault.

No source or product change was made from this run. The durable implementation
work needed next is to make `/normal` the unambiguous shipped Browser entry (or
make `/` serve the same artifact), then re-run only the scan/reconfigure evidence
through that page so the already-added driver instrumentation can answer B.
