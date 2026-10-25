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

