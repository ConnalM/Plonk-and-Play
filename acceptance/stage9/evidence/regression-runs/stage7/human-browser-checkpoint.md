# Stage 7 human Browser checkpoint — PASS

**Status:** PASS — manually verified through the Wokwi Private IoT Gateway

The checkpoint passed after the gateway/browser session recovered. Stage 7 is
accepted against its frozen acceptance plan.

## What was demonstrated

- The manually uploaded Stage 7 demonstration image emitted the unmistakable
  startup diagnostic `P&P STAGE 7 DEMO`.
- Wokwi Serial Monitor therefore established that the current Stage 7 demo
  firmware was running.
- Independent inspection of two Wokwi PCAP captures established that the
  simulated ESP32 joined the private gateway network as `10.13.37.2`, listened
  on TCP port 80, received gateway-originated SYN packets and sent valid
  SYN/ACK responses.

## External blocker

The gateway/Wokwi forwarding side did not complete the target TCP handshake.
In one capture it reset immediately after the ESP32 SYN/ACK; in a fresh
gateway/browser session it did not send the final ACK after the SYN/ACK.
Gateway v2.0.1 was running with the verified mapping
`--forward 9080:10.13.37.2:80`. Its retained log reports failed target dials,
including `no route to host` before the active browser client was connected.

## Completed human observation

The real rendered Browser was observed through the Private IoT Gateway. It
remained visible while current State refreshed through the race, showed laps
`0 -> 1 -> 2 -> FINISHED`, displayed the corresponding last-lap values and a
separate `LAP_COMPLETED` Fact/Event, and returned to `synchronised` after each
successful authoritative State refresh. A Ctrl+F5 fresh Browser instance then
reconstructed the existing authoritative FINISHED State and the latest Fact
without another race event.

## Retained local material

- `gateway.stdout.log` and `gateway.stderr.log` preserve the replacement
  gateway output and errors.
- `gateway-session-1.pcap` — SHA-256
  `46654FA47B7202A7577460D8BF1D828C833104AFCF75FC29759B374F0DFDA3F1`.
- `gateway-session-2.pcap` — SHA-256
  `EA230C823884A6A7A64D91AFD415FC952ABAD590ADAD8AF5D62D9CED77C4E76B`.
- `gateway-pcap-archive.zip` — SHA-256
  `C15EEC285BC493A4BB2508EE116083C6B2C0CFF6EDE95309133A531289244D75`.

The historical gateway failure evidence remains retained because it records a
real external test-environment interruption; it does not invalidate the later
successful human checkpoint.
