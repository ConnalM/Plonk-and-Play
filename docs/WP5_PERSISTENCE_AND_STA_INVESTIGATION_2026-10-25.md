# WP5 persistence and STA investigation — 2026-10-25

This is an investigation record only. No Results, Browser, Race Engine,
History reconstruction, persistence behaviour, or networking behaviour was
changed by this investigation.

## Physical evidence

The relevant run is recorded in
`firmware/logs/device-monitor-261024-175616.log`. It reached the intended
two-lap All Stop sequence:

- Lane 1 lap 1 accepted.
- Lane 2 lap 1 accepted.
- Lane 1 lap 2 accepted.
- `finish_settled` and a sealed, integrity-OK result were emitted.
- No `HistoryStored` event was emitted.
- The terminal diagnostic was:
  `history_storage_fault reason=3(detail_finalize)`.

The controller did not panic or reboot. The repeated
`/pp-diagnostics/records.roll.tmp` removal messages are diagnostic rolling
cleanup noise and are not the reported persistence boundary.

## Detail-finalisation root cause

`SdActiveDetailStore::writeRecord()` in
`firmware/include/pp/sd_store_arduino.h` opens each new active-detail file
with `FILE_APPEND` and writes the 24-byte detail header only when that open
returns false. The ESP32 Arduino FS framework defines `FILE_APPEND` as `"a"`.
Its VFS open path permits creation for any non-read mode, and the underlying
`fopen(path, "a")` creates a missing file and returns a valid handle.

Therefore the first record for a new lane file is written without the required
24-byte header. `sealFile()` then requires the file size to be
`HeaderBytes + count * RecordBytes` and validates the header. The physical
files are short and/or headerless, so `sealFile()` returns false and
`finalize()` publishes `detail_finalize` before `appendDetailed()` is reached.

This explains the real run without requiring a speculative SD or Results
diagnosis. The smallest source correction is to determine file existence
explicitly (or otherwise use a creation path that writes the header exactly
once), then append records to an existing file. Existing active-detail format,
marker verification, History format, sequence semantics, ownership boundary
and failure isolation remain unchanged. No hardware retest is requested until
that narrowly scoped correction is reviewed and built.

## Separate STA/networking issue

The same log shows a healthy initial remembered association:

- STA connected and obtained `192.168.68.113`.
- The AP remained started at `192.168.4.1` and had a client.

After Reconfigure, the route was received by the controlling Browser and
accepted with a non-empty SSID and password presence. The driver then reported
`STA_EVENT ... DISCONNECTED reason=15 name=4WAY_HANDSHAKE_TIMEOUT`, repeatedly,
with `LOST_IP` also observed. The source passes the supplied credentials to
`WiFi.begin(ssid, password)` and does not transform or log the password.

The demonstrated failure is therefore at the WPA four-way authentication
handshake, after request/lease/validation acceptance. It is not an AP startup
failure, authority rejection, credential-store absence, or Results/persistence
failure. The exact underlying cause (wrong supplied PSK, changed router
credentials/security, or a lower-level RF/association problem) cannot be
distinguished without exposing credentials, which is prohibited.

The interrupted `/normal` responses are HTTP response-send failures during the
network churn; they do not show that the P&P AP stopped. This STA issue remains
separate from the detail-finalisation defect.

## Smallest proof tests

1. After the detail-file creation correction, run one direct-AP two-lap All
   Stop race and require `HistoryStored` with no `detail_finalize` fault.
2. For STA, use one controlling Browser over the AP and submit one known-good
   external SSID/password. A successful `STA_EVENT CONNECTED` followed by
   `GOT_IP` proves the workflow; another reason-15 timeout leaves the fault at
   the authentication/router boundary for separate networking diagnosis.

## 0e97856 instrumented physical run — 2026-10-25 17:05:52

The continuous capture is
`firmware/logs/device-monitor-261025-170552.log`, from the verified
`0e978562f9a61b931295a4b3334d7f4fc93b327d` / `stage8bp1s3sd` image. The run
completed the intended two-lane, two-lap All Stop sequence and emitted the
sealed, integrity-OK result, but did not store History.

The direct persistence trace is:

1. `detail_finalize begin ... queue=0 pending=0 mounted=1 dropped=0`;
2. `detail_trace stage=finalize substage=drain_end reason=complete`;
3. `detail_trace stage=seal substage=active_size reason=active_size`;
4. `detail_finalize failed stage=seal entry=0 reason=active_size verify=none`;
5. `history_storage_fault reason=3(detail_finalize)`.

There is no `HistoryStored`, no drain/write failure, no marker or verification
trace, and no panic/reset. The exact proven failure boundary is therefore the
active-detail seal size invariant for entry 0. For two records the required
size is `24 + (2 * 40) = 104` bytes; this candidate did not print the actual
file size, so the lower-level reason for that mismatch is not established by
this capture and must not be guessed from the result screen. The next
diagnostic step, if authorised, is to report expected and observed sizes at the
seal check; no Results or Browser change is indicated by this evidence.

Because finalisation failed, Race Engine correctly latched
`DetailFinalize` and did not publish `HistoryStored`. The Browser consequently
received a sealed result whose per-entry `finishDurationUs` remained zero;
BEST/progress/gap remained available. This run does not show a Browser timing
or Results reconstruction fault.

The repeated `/pp-diagnostics/records.roll.tmp` removal errors are separate
diagnostic rolling cleanup noise. They are interleaved throughout the run but
are not the reported detail-finalisation boundary. The one
`active-1-1.tmp ... no permits for creation` line is the existing read-only
existence probe before the active-detail creation path and is not itself a
reported write failure.

Secondary evidence: `/normal` emitted five chunk traces (7 ms, 415 ms, 66 ms,
43 ms, 49 ms) and then failed on chunk 4 with result `45062`, total 582 ms.
This capture contains no 30-second `/normal` response. Context traces identify
two clients: `client_tag=1` is master and `client_tag=2` is spectator, both
with `owner_connected=true`; no authority contradiction is shown.

