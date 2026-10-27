# HTTP/SSE Isolation Diagnostic — 2026-10-27 14:08:59

## Status

**INCONCLUSIVE / STOPPED — not a validation of the approved candidate.**

The upload command was run from the HTTP/SSE worktree after its branch had advanced to
`16ae8107346c94eecf4d61fe96bf25dacf5a8e42` (the evidence commit).  The boot provenance
therefore identifies `16ae810…`, not the approved implementation commit
`56ee14a0fba4b4f9549ee0dc4ee8cd43be4f11f1`.

The approved artifact identity was:

- environment: `stage8bp1s3sd`
- size: 1,478,656 bytes
- SHA-256: `C53F29910C8B51FEBA0ECE1615A9ADB2C17934F2AA8163A3EC27D0CCD8D2FDB5`

The image actually rebuilt/uploaded from the current worktree was:

- runtime source commit: `16ae8107346c94eecf4d61fe96bf25dacf5a8e42`
- runtime ELF SHA prefix: `f04601d38`
- local rebuilt `firmware.bin` SHA-256: `00BE360520A5F959252EDE91AD33D3DD25E354718F8D7EE422FB1D26EEF927D9`

## Capture

Raw capture: `firmware/logs/device-monitor-261027-140859.log`.

Boot reached SD mount, AP/mDNS startup and the remembered-STA path.  Before any
transport-pressure client was created, the diagnostic SSE workers emitted idle HWM lines
and `pp_sse_3` hit a stack-canary panic:

```text
[SSE_STACK] generation=0 fd=-1 stage=idle hwm_words=1520 stack_words=2048 cleanup=none
Guru Meditation Error: Core  0 panic'ed (Unhandled debug exception).
Debug exception reason: Stack canary watchpoint triggered (pp_sse_3)
Rebooting...
```

The subsequent automatic reboot completed normal startup and emitted additional idle
HWM lines, but no page/SSE pressure scenario was run.  No race or persistence operation
was performed.  The capture contains no evidence of a transport-isolation acceptance
result.

## Consequence

This record must not be used to select the final SSE stack size or to claim NET15.14 /
Stage 10 10.10a physical validation.  A new run requires the exact approved
`56ee14a…` image (SHA above), with boot provenance checked before any client action.
