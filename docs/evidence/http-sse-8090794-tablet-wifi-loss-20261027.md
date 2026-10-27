# HTTP/SSE tablet Wi-Fi-loss observation — 8090794

Date: 2026-10-27

## Setup and baseline

Capture: `firmware/logs/device-monitor-8090794-reconnect-test2.log`.

The controller was power-cycled to restore the remembered house-Wi-Fi association. Boot evidence showed SD mount OK, AP/mDNS/HTTP ready, source commit `8090794f134713e5c7e21943f1dbc329491680a5`, environment `stage8bp1s3sd`, and STA `GOT_IP ip=192.168.68.102`.

After Browser recovery settled, two SSE sessions were healthy. Context evidence identified the laptop as spectator (`client_tag=1`, `is_master=false`) and the tablet as Master (`client_tag=2`, `is_master=true`). The controller remained stable and no race/persistence activity occurred.

## Tablet Wi-Fi-off observation

With the live capture already running, the tablet's Wi-Fi was switched off while its P&P page was left open and untouched. For approximately three minutes afterwards:

- both SSE workers continued reporting keepalive;
- no SSE replacement, send failure, disconnect, or reconnect request occurred;
- no HTTPD client-count growth or ordinary-request failure occurred;
- the controller continued SD self-test PASS output;
- no panic, reset, authority mutation, race, or persistence activity occurred.

The observation therefore provides no server-visible tablet disconnect boundary. It is not claimed as a reconnect PASS or FAIL; the TCP/SSE session remained apparently alive despite the tablet Wi-Fi being reported off.

The earlier post-reboot recovery section does contain bounded 503/close responses at client counts 9 and 10, followed by successful re-admission at client count 8. Those events occurred during automatic Browser recovery before the tablet Wi-Fi-off action and are retained as context, not attributed to the deliberate loss observation.

