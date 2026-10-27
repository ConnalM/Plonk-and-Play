# HTTP/SSE two-Browser reboot recovery — 8090794

Date: 2026-10-27

Capture: `firmware/logs/device-monitor-8090794-reboot-recovery.log`.

## Pre-reboot baseline

Two SSE workers were healthy and both Browsers were synchronised. Read-only `/health` and `/context` were successful at HTTPD population 13; the laptop was spectator and the tablet retained Master/Race Director authority.

## Boot and recovery

The deliberate controller power-cycle produced a normal boot with:

- source commit `8090794f134713e5c7e21943f1dbc329491680a5`;
- environment `stage8bp1s3sd`;
- SD mount OK, AP/mDNS/HTTP ready, STA `GOT_IP ip=192.168.68.102`;
- no panic or unexpected reset after boot.

Both Browser pages initiated recovery. Fresh `/events` admissions occurred at client counts 1, 2 and 8, with fresh state/proposal/context acquisition. Context evidence reconstructed the laptop as spectator (`client_tag=1`) and the tablet as Master (`client_tag=2`).

## First failure boundary

After the two sessions appeared recovered, one SSE worker reported:

```
[SSE_STACK] generation=4 fd=51 stage=send_failure ... cleanup=send_failure
```

The following reconnect attempts were rejected at the corrected reserve boundary:

```
clients=10 ... outcome=ordinary_capacity_exhausted
clients=9  ... outcome=ordinary_capacity_exhausted
```

Both rejections returned result `257` through the explicit bounded rejection path. The other SSE remained healthy, but the failed Browser did not complete automatic recovery within the observed interval. The test was stopped immediately; no Browser was refreshed, no retry was initiated, and no race or persistence mutation occurred.

Overall result: **FAIL — automatic two-Browser reboot recovery did not remain stable after the first post-reboot SSE send failure.**

