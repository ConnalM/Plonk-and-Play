# WP5 LAN Reachability / HTTP Investigation

## Scope

Read-only investigation of the `192.168.68.120` disappearance observed while
the Normal Browser was recovering. No firmware or network configuration was
changed.

## Evidence

The boot in `firmware/logs/device-monitor-261025-180059.log` reports one clean
boot, SD owner mount success, AP startup, remembered STA configuration,
`STA_EVENT CONNECTED`, and `STA_EVENT GOT_IP ip=192.168.68.120`. The capture
contains no panic, reset header, `LOST_IP`, or STA `DISCONNECTED` event.

The same capture records a Normal Browser response taking 64,809 ms. Individual
4,096-byte sends took up to 10,543 ms. Two later Normal Browser attempts ended
with `result=45062` after 273 ms and 10,684 ms. These are confirmed HTTP send
failures/slow sends.

The AP client count changed from one to zero and back to one at approximately
621 seconds, showing AP-client churn but not a controller reboot. It does not
prove a STA failure.

During the investigation the PC could initially reach `/health` and
`/proposal` at `192.168.68.120`, while `/context`, `/state`, and
`/network/status` timed out. Later `/health` and `/proposal` also timed out;
the PC ARP table no longer contained `192.168.68.120`, and ping reported the
host unreachable. The host remained on the house LAN.

## Interpretation

The Browser startup poll requests `/state`, `/proposal`, and `/context`
concurrently and has a four-second request timeout. Any failed request marks
the Browser offline. The `/context` handler is an in-memory lease read and the
`/state` handler takes an in-memory Noticeboard snapshot and serializes it;
neither performs SD or History work. Their timeout is therefore not an
authoritative rejection and cannot be explained by storage reconstruction.

The 64.8-second `/normal` transfer is a real HTTP-service problem. The current
implementation sends the page in repeated `httpd_resp_send_chunk()` calls from
the HTTP route, with the HTTP server send wait timeout set to five seconds.
While a slow client holds that route, other HTTP requests are delayed by the
single HTTP server execution path. This is sufficient to explain Browser
state-request timeouts during a slow page transfer.

It does **not** explain the later loss of ARP reachability. HTTP send
starvation can delay or fail TCP responses, but the current evidence does not
show it removing the STA interface or changing the controller's IP. There is
no captured STA event identifying the cause of the IP/ARP disappearance.

## Finding

1. HTTP starvation during `/normal` is proven and is a separate serious HTTP
   transport defect.
2. Controller LAN reachability at `192.168.68.120` was subsequently lost, but
   the available capture cannot distinguish an unreported STA/link failure,
   DHCP/address/interface loss, or an external LAN condition.
3. No evidence supports a firmware reboot, panic, SD failure, or authority
   decision as the cause of the disappearance.
4. A follow-up capture must expose periodic authoritative STA state/IP and
   HTTP server state if the LAN disappearance is to be classified without
   another speculative fix.

## Status

No implementation change is proposed by this investigation. The separate
`records.roll.tmp` removal messages remain unrelated noise unless later
evidence links them to the network failure.
