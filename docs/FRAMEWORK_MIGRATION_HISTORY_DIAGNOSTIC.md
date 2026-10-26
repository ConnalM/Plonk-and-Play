# Framework migration History diagnostic

Date: 2026-10-26  
Firmware: `7ffeed91b4869f2e7d00a400ad7ba68b0f0545ca`  
Environment: `stage8bp1s3sd`  
Capture: `firmware/logs/device-monitor-history-diag-com7-261026-151500.log`

## Controlled observation

The reviewed image booted on COM7 with the expected source commit, environment,
and populated application ELF hash. The SD owner mounted successfully. One
non-destructive `GET /history` was issued; no race or filesystem mutation was
performed.

The client observed `The response ended prematurely. (ResponseEnded)`.

Serial reported:

```text
[P8-P1] history_payload_decode length=338 checksum_ok=1 magic=0x50534631 wire_version=3 declared=338 format=3 expected_format=3 sealed=1
[DEV] HTTP ERROR route=/history duration_ms=39 result=260 active=0 heap=196836 min_heap=191808
```

There was no `history_payload_read` fault line. That means the selected payload
opened, its observed size matched the metadata-expected size, and the complete
read returned successfully. The first failing boundary is therefore
`VersionedResultCodec::decode()` after checksum and header/schema validation.

The diagnostic proves the payload has a valid checksum, PSF1 magic, wire version
3, declared length 338, result format 3 and sealed flag. It does not identify a
later field/record parse failure or final reader-position mismatch; those remain
the next diagnostic distinction if further work is approved.

No production correction is proposed by this observation. Display and HTTP/SSE
remain out of scope.
