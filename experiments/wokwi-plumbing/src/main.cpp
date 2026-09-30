#include <Arduino.h>
#include <WiFi.h>
#include <esp_http_server.h>

// Temporary plumbing probe only. No P&P architecture or application logic.
namespace {
constexpr char kProbeId[] = "pp-wokwi-plumbing-v1";
constexpr size_t kMaxEchoBytes = 1024;
httpd_handle_t server = nullptr;

const char kPage[] = R"HTML(<!doctype html>
<html lang="en">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>P&amp;P temporary plumbing probe</title>
<style>
body{font:18px system-ui;max-width:42rem;margin:3rem auto;padding:0 1rem}
input,button{font:inherit;padding:.4rem} pre{white-space:pre-wrap;overflow-wrap:anywhere}
</style>
<h1>P&amp;P temporary plumbing probe</h1>
<p>This is a disposable connectivity test, not the P&amp;P application.</p>
<p id="probe-id">pp-wokwi-plumbing-v1</p>
<p>WebSocket: <strong id="status">connecting</strong></p>
<form id="echo-form">
  <label for="message">Message</label>
  <input id="message" value="hello from browser" maxlength="256">
  <button id="send" disabled>Send echo</button>
</form>
<p>Last echo: <output id="last-echo"></output></p>
<p>Echo count: <output id="echo-count">0</output></p>
<pre id="log" aria-live="polite"></pre>
<script>
const status = document.getElementById('status');
const send = document.getElementById('send');
const log = document.getElementById('log');
const lines = [];
function record(text) {
  lines.push(text);
  if (lines.length > 20) lines.shift();
  log.textContent = lines.join('\n');
}
// Relative to the page origin: also works through a forwarded localhost port.
const socket = new WebSocket(`${location.protocol === 'https:' ? 'wss' : 'ws'}://${location.host}/ws`);
let echoes = 0;
socket.addEventListener('open', () => {
  status.textContent = 'open';
  send.disabled = false;
  record('WebSocket open');
});
socket.addEventListener('message', event => {
  document.getElementById('last-echo').textContent = event.data;
  document.getElementById('echo-count').textContent = String(++echoes);
  record(`received: ${event.data}`);
});
socket.addEventListener('error', () => {
  status.textContent = 'error';
  send.disabled = true;
  record('WebSocket error');
});
socket.addEventListener('close', event => {
  status.textContent = 'closed';
  send.disabled = true;
  record(`WebSocket closed: ${event.code}`);
});
document.getElementById('echo-form').addEventListener('submit', event => {
  event.preventDefault();
  if (socket.readyState !== WebSocket.OPEN) return;
  const message = document.getElementById('message').value;
  if (new TextEncoder().encode(message).length > 1024) {
    record('Message exceeds 1024 UTF-8 bytes');
    return;
  }
  socket.send(message);
  record(`sent: ${message}`);
});
</script>
</html>)HTML";

esp_err_t pageHandler(httpd_req_t *request) {
  Serial.printf("HTTP_GET path=/ fd=%d\n", httpd_req_to_sockfd(request));
  httpd_resp_set_type(request, "text/html; charset=utf-8");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  return httpd_resp_send(request, kPage, sizeof(kPage) - 1);
}

esp_err_t healthHandler(httpd_req_t *request) {
  httpd_resp_set_type(request, "application/json");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  return httpd_resp_sendstr(request,
      "{\"ok\":true,\"probe\":\"pp-wokwi-plumbing-v1\",\"websocket\":\"/ws\"}");
}

esp_err_t echoHandler(httpd_req_t *request) {
  const int fd = httpd_req_to_sockfd(request);
  if (request->method == HTTP_GET) {
    Serial.printf("WS_OPEN fd=%d\n", fd);
    return ESP_OK;
  }

  httpd_ws_frame_t frame = {};
  esp_err_t result = httpd_ws_recv_frame(request, &frame, 0);
  if (result != ESP_OK) {
    Serial.printf("WS_RECEIVE_ERROR fd=%d error=%s\n", fd, esp_err_to_name(result));
    return result;
  }
  // Keep this probe bounded. It supports complete text/binary messages only.
  if (frame.len > kMaxEchoBytes || !frame.final ||
      (frame.type != HTTPD_WS_TYPE_TEXT && frame.type != HTTPD_WS_TYPE_BINARY)) {
    Serial.printf("WS_REJECT fd=%d bytes=%u type=%d final=%d\n", fd,
                  static_cast<unsigned>(frame.len), frame.type, frame.final);
    return ESP_FAIL; // HTTP server closes this session, leaving other clients intact.
  }

  uint8_t payload[kMaxEchoBytes];
  frame.payload = payload;
  if (frame.len > 0) {
    result = httpd_ws_recv_frame(request, &frame, sizeof(payload));
    if (result != ESP_OK) {
      Serial.printf("WS_PAYLOAD_ERROR fd=%d error=%s\n", fd, esp_err_to_name(result));
      return result;
    }
  }
  result = httpd_ws_send_frame(request, &frame);
  Serial.printf("WS_ECHO fd=%d bytes=%u result=%s\n", fd,
                static_cast<unsigned>(frame.len), esp_err_to_name(result));
  return result;
}

bool startServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.stack_size = 8192;
  config.max_open_sockets = 7; // Two persistent WebSockets plus HTTP requests.
  config.lru_purge_enable = false; // Do not silently evict another test client.
  esp_err_t result = httpd_start(&server, &config);
  if (result != ESP_OK) {
    Serial.printf("HTTP_START_ERROR error=%s\n", esp_err_to_name(result));
    return false;
  }

  httpd_uri_t page = {};
  page.uri = "/";
  page.method = HTTP_GET;
  page.handler = pageHandler;
  httpd_uri_t health = {};
  health.uri = "/health";
  health.method = HTTP_GET;
  health.handler = healthHandler;
  httpd_uri_t echo = {};
  echo.uri = "/ws";
  echo.method = HTTP_GET;
  echo.handler = echoHandler;
  echo.is_websocket = true;
  // The framework handles WebSocket ping/pong and close control frames.
  for (const auto *route : {&page, &health, &echo}) {
    result = httpd_register_uri_handler(server, route);
    if (result != ESP_OK) {
      Serial.printf("HTTP_ROUTE_ERROR path=%s error=%s\n", route->uri, esp_err_to_name(result));
      httpd_stop(server);
      server = nullptr;
      return false;
    }
  }
  return true;
}
} // namespace

void setup() {
  Serial.begin(115200);
  Serial.printf("\nPROBE_BOOT id=%s\n", kProbeId);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin("Wokwi-GUEST", "", 6);
  Serial.println("WIFI_CONNECTING ssid=Wokwi-GUEST channel=6");
  const unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 30000) {
    delay(250);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WIFI_TIMEOUT restarting_in_ms=3000");
    delay(3000);
    ESP.restart();
    return;
  }
  Serial.printf("WIFI_CONNECTED ip=%s\n", WiFi.localIP().toString().c_str());
  if (startServer()) {
    Serial.println("PROBE_READY id=pp-wokwi-plumbing-v1 http_port=80 ws_path=/ws");
  }
}

void loop() {
  // HTTP/WebSocket processing runs in the framework's dedicated server task.
  static bool wasConnected = true;
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != wasConnected) {
    Serial.println(connected ? "WIFI_RECONNECTED" : "WIFI_DISCONNECTED");
    wasConnected = connected;
  }
  delay(100);
}
