# SD-2 mounted History integrity checkpoint — 388a5f3

Date: 2026-10-28  
Firmware: `388a5f3e6fe535c9b78a3af21c47034ffebdf5d2`  
Environment: `stage8bp1s3sd`  
Capture: `device-monitor-388a5f3-history-integrity.log`  
Capture SHA-256: `F5069F9E0D6785DE238EBEB461A8C587E461C92E5D485CF08DD1B2F6C9913CEE`

## Result

**PASS — one production History request completed with valid siblings preserved.**

Connal observed the tablet History screen displaying 11 races. The request did
not present `result=260` or another whole-route failure.

## Integrity evidence

Startup/reconstruction emitted bounded exclusions, without repair or deletion:

- index 2, sequence 9, reason `corrupt`;
- index 3, sequence 8, reason `corrupt`.

The valid retained records were returned to the tablet; the 11-race History
view is the physical proof that enumeration continued past the invalid entries.

## Storage and transport state

- SD owner remained physically mounted/healthy.
- Logical History remained integrity-degraded because excluded committed entries
  are retained and reported as corrupt; no physical-owner degradation occurred.
- No remount, Retry-storage request, retry storm, panic, reset, race mutation,
  authority mutation, or Browser/SSE failure was observed.
- The capture contained no `/history` error marker after the request.

The capture was frozen after this single request; no further controller, SD,
Browser, race, or recovery action was performed.
