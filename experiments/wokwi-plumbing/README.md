# Temporary ESP32 / Wokwi plumbing probe

Disposable experiment, **not the P&P implementation**, not an architectural
decision and not a final hardware selection. All its files and generated output
stay under `experiments/wokwi-plumbing`. The real project documentation is unchanged.

## What the firmware does

- Joins Wokwi's `Wokwi-GUEST` Wi-Fi (empty password, channel 6).
- Serves a self-contained HTML page on port 80 at `/`.
- Serves JSON at `/health`, including `probe: "pp-wokwi-plumbing-v1"`.
- Accepts WebSocket connections at `/ws` on the **same port**. Echoes complete
  text or binary messages up to 1,024 bytes back to the originating connection.
  Fragmented/oversized messages are outside this probe's scope and close that
  connection. Ping/pong and close handling belong to the framework.
- Allows seven server sockets: enough for two persistent WebSockets plus HTTP
  requests. It never deliberately evicts an existing socket to admit another.
- Logs boot, Wi-Fi readiness, HTTP requests, WebSocket opens, echoes and errors
  at 115200 baud. Socket IDs distinguish simultaneous WebSocket clients.

The page opens one WebSocket, provides an echo form, and exposes stable element
IDs (`status`, `message`, `send`, `last-echo`, `echo-count`) for later browser tests.
It derives the WebSocket address from the page origin, including the forwarded
port. It uses no CDN or external JavaScript dependency.

## Local build (no simulator required)

From PowerShell, run `./build.ps1` in this folder. The script uses the existing
`$env:USERPROFILE/.platformio` installation, performs a PlatformIO build and
merges the bootloader, partitions, OTA initialization and application using the
existing esptool. It does not invoke a software installer.

Installed baseline: PlatformIO Core 6.1.18, Espressif32 platform 6.9.0,
Arduino ESP32 2.0.17, Xtensa GCC 8.4.0, esptool 4.5.1.
No third-party `lib_deps`; HTTP/WebSockets use the framework's `esp_http_server`.
Do not approve package downloads if a different machine lacks this baseline.

Main generated files (ignored by Git):

- `.pio/build/esp32dev/firmware.elf`: linked executable with symbols.
- `.pio/build/esp32dev/firmware.bin`: application image.
- `.pio/build/esp32dev/bootloader.bin` and `partitions.bin`.
- `.pio/build/esp32dev/firmware.merged.bin`: **use this for browser loading**;
  includes the full boot layout, with offsets relative to flash address zero.
- Other `.pio/` files are compiler objects, archives and build metadata.

Verified locally on 2026-09-30: build and merge succeeded. PlatformIO reported
43,416 bytes RAM (13.2%) and 760,705 bytes flash (58.0% of the app partition).
The merged image is 832,816 bytes. Its four constituent images were checked
byte-for-byte at their flash offsets, and the application fits its partition.
The page's JavaScript passed a syntax check. Subsequent simulator results are
recorded below; no physical hardware was tested. `.pio/generated-files.txt` records the generated file inventory
at completion of this build; it is an audit snapshot, not a build input.

## End-to-end result: 2026-09-30

All six requested plumbing checks passed. The local PlatformIO build was loaded
into Wokwi's ESP32 browser simulator. The official Private IoT Gateway v2.0.1
reported a connected Wokwi client and forwarded local port 9080 to
10.13.37.2:80. At 14:03:44-45 UTC, `test-network.cjs` verified:

- HTTP `/health`: status 200 and exact expected JSON.
- `/ws`: exact echo of `single-client: exact echo 0123`.
- Two simultaneously open, independent WebSocket connections: exact echoes of
  `client-A: alpha-123` and `client-B: beta-456`, respectively.
- After closing A, B still echoed `client-B: still alive after A disconnects`.
- Received-message lists matched each client's own messages, without cross-talk.

Machine-readable evidence: `.pio/network-test-results.json`. Gateway output:
`.pio/gateway.stdout.log`. These generated files are ignored by Git.
Run `node test-network.cjs` (Node 22+) to repeat the protocol checks while the
gateway and simulator are running; no extra Node packages are required.

Gateway installation on this computer:
`C:\Users\conna\.wokwi\gateway-v2.0.1\wokwigw.exe`, started with
`--forward 9080:10.13.37.2:80`.

Automation limitation: firmware upload required manual file selection. The
current command is **Upload Firmware and Start Simulation...**; the old Load HEX
command only displays a rename notice. Automatic attachment with the current
command failed with a missing browser node. Wokwi's Restart button recompiled
the template instead of retaining the uploaded binary; upload the merged binary
again when starting a new run. Private gateway selection and browser permissions
also required user interaction. Protocol checks ran automatically without VS Code.
The two tested clients were Node WebSocket connections, not two rendered browser
contexts. The broader browser UI acceptance plan below remains future work.

## Broader browser acceptance plan

Prerequisites: a gateway-enabled Wokwi account, the Windows Private IoT Gateway,
and browser automation. Chrome and a bundled Playwright are already available
on the inspected computer. Nothing here installs, launches or authenticates Wokwi.

1. **Automatic firmware loading:** use Wokwi's ESP32 custom-firmware browser
   template and the board/serial wiring in `diagram.json`. Automate its
   `Upload Firmware and Start Simulation` action with a file-chooser handler
   supplying `firmware.merged.bin`; never use the browser's cloud compiler.
   Capture serial output and require `PROBE_BOOT id=pp-wokwi-plumbing-v1`,
   `WIFI_CONNECTED`, then `PROBE_READY`. This is the first validation gate:
   the Wokwi-specific automation has not yet been implemented or proven.
2. **Network connection:** launch the private gateway and enable it in the Wokwi
   browser session. Forward localhost port 9080 to the simulated device's port
   80 (the standard gateway default; explicit equivalent is
   `wokwigw --forward 9080:10.13.37.2:80`). Keep that simulator session running.
   Wokwi's public gateway and the current CLI do not provide this inbound route.
3. **HTTP:** poll `http://localhost:9080/health` with a bounded timeout, require
   HTTP 200 and the exact probe ID; load `/` and check the heading and probe ID.
   Capture the response and serial log on failure.
4. **WebSocket:** connect to `ws://localhost:9080/ws`, require the successful
   upgrade, send a unique nonce and require the identical echo. Also check an
   empty text message and UTF-8 text. Apply bounded connection/message timeouts.
5. **Two simultaneous browser clients:** open `/` in two independent browser
   contexts. Wait until BOTH show `status=open` before sending anything. Send
   distinct A/B nonces through their forms, repeatedly and with overlapping
   activity. Each `last-echo` must match only its own message; counts must rise
   independently. Keep both connections open while fetching `/health`. Close A,
   then verify B still echoes. Reopen A and verify both again. Record browser
   console/page errors, screenshots, network failures and serial socket IDs.

The completed protocol checks prove Wi-Fi, gateway forwarding, WebSocket runtime
behaviour and simultaneous network clients for this probe. Automatic firmware
upload and the broader rendered-browser checks above are not yet proven.
This probe does not implement race logic, persistence,
authentication, discovery, TLS or any P&P product interface.

References:
- https://docs.wokwi.com/guides/esp32#custom-application-firmware
- https://docs.wokwi.com/guides/esp32-wifi
- https://github.com/wokwi/wokwigw
- https://playwright.dev/docs/input#upload-files
- https://playwright.dev/docs/browser-contexts
