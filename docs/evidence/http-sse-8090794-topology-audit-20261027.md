# HTTP/SSE Browser-topology audit — 8090794

Date: 2026-10-27

## Untouched baseline

Fresh capture: `firmware/logs/device-monitor-8090794-topology-audit.log`.

Before Browser inspection, the controller was alive with SD self-test PASS, one existing SSE worker (`generation=3`, fd 51), and HTTPD population 7. Read-only `/health` and `/context` returned 200; `/context` reported spectator/no owner. No race or persistence activity was present.

## Device mapping

- Laptop: one Chrome P&P tab at `plonk-and-play.local`; visible spectator presentation.
- Tablet: one P&P tab at `http://192.168.68.102/`; after connecting to house Wi-Fi, the capture showed its root request and a second admitted SSE.

The tablet's deliberate **MAKE RACE DIRECTOR** action was observed as accepted (`client_tag=5`, `is_master=true`). The laptop remained spectator. Two SSE sessions were present.

## Invalid connectivity action and stop

The subsequent capture did not contain a tablet-side Wi-Fi loss. Instead it contains:

```
[WIFI] BROWSER_OBSERVATION kind=network_control_state detail=1 client_is_master=true
[DEV] HTTP ERROR route=/network/disconnect duration_ms=12 result=45062
[WIFI] STA_EVENT event=DISCONNECTED reason=8 name=ASSOC_LEAVE
```

This is the controller's Browser Network disconnect action, not a physical tablet Wi-Fi disconnect. The validation was stopped immediately. No reconnect test result or reboot-recovery result is claimed, and no further Browser action was requested.

