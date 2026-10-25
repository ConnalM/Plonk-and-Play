#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_wifi.h>

namespace pp {

// The Arduino/ESP-IDF STA configuration is the sole persistent credential
// store.  This manager keeps only non-secret runtime state and never reads or
// emits the STA password.
class ExternalWifiManager {
public:
  static constexpr const char* FactoryApSsid = "Plonk-and-Play";
  static constexpr const char* MdnsHost = "plonk-and-play.local";

  enum class StaState : uint8_t {
    Unconfigured,
    Disconnected,
    Connecting,
    Connected,
  };

  enum class StaFailure : uint8_t {
    None,
    NoSsidAvailable,
    ConnectFailed,
    ConnectionLost,
  };

  enum class ScanState : uint8_t {
    Idle,
    Running,
    Complete,
    Unavailable,
  };

  static const char* staStateName(StaState state) {
    switch (state) {
      case StaState::Unconfigured: return "UNCONFIGURED";
      case StaState::Disconnected: return "DISCONNECTED";
      case StaState::Connecting: return "CONNECTING";
      case StaState::Connected: return "CONNECTED";
    }
    return "DISCONNECTED";
  }

  static const char* staFailureName(StaFailure failure) {
    switch (failure) {
      case StaFailure::None: return "NONE";
      case StaFailure::NoSsidAvailable: return "NO_SSID_AVAILABLE";
      case StaFailure::ConnectFailed: return "CONNECT_FAILED";
      case StaFailure::ConnectionLost: return "CONNECTION_LOST";
    }
    return "UNKNOWN";
  }

  static const char* scanStateName(ScanState state) {
    switch (state) {
      case ScanState::Idle: return "IDLE";
      case ScanState::Running: return "RUNNING";
      case ScanState::Complete: return "COMPLETE";
      case ScanState::Unavailable: return "UNAVAILABLE";
    }
    return "UNAVAILABLE";
  }

  struct Status {
    bool apStarted = false;
    bool staConfigured = false;
    bool staConnected = false;
    bool reconnectSuppressed = false;
    bool mdnsStarted = false;
    bool scanRunning = false;
    int16_t scanCount = -2;
    ScanState scanState = ScanState::Idle;
    uint32_t scanRequests = 0;
    uint32_t scanAccepted = 0;
    uint32_t scanCompleted = 0;
    uint32_t scanUnavailable = 0;
    int16_t scanLastResult = -2;
    uint8_t apClients = 0;
    int32_t staRssi = 0;
    uint32_t reconnectAttempts = 0;
    StaState staState = StaState::Unconfigured;
    StaFailure staFailure = StaFailure::None;
    String apIp;
    String staIp;
    String staSsid;
  };

  void begin() {
    WiFi.persistent(true);
    WiFi.mode(WIFI_AP_STA);
    installEventDiagnostics();
    apStarted_ = WiFi.softAP(FactoryApSsid);
    suppressed_ = false;
    stationConfigured_ = hasStoredStationConfiguration();
    staFailure_ = StaFailure::None;
    if (stationConfigured_) {
      staState_ = StaState::Connecting;
      WiFi.setAutoReconnect(true);
      (void)WiFi.begin();
      lastAttemptMs_ = millis();
    } else {
      staState_ = StaState::Unconfigured;
    }
    startMdns();
    Serial.printf("[WIFI] P&P AP %s ssid=%s ip=%s clients=%u\n",
                  apStarted_ ? "STARTED" : "FAILED", FactoryApSsid,
                  WiFi.softAPIP().toString().c_str(),
                  static_cast<unsigned>(WiFi.softAPgetStationNum()));
    if (stationConfigured_) {
      Serial.println("[WIFI] external STA remembered configuration detected");
    }
  }

  void tick() {
    const uint32_t now = millis();
    if (!apStarted_ && static_cast<uint32_t>(now - lastApAttemptMs_) >= 5000U) {
      lastApAttemptMs_ = now;
      apStarted_ = WiFi.softAP(FactoryApSsid);
      Serial.printf("[WIFI] P&P AP %s ssid=%s ip=%s clients=%u\n",
                    apStarted_ ? "STARTED" : "FAILED", FactoryApSsid,
                    WiFi.softAPIP().toString().c_str(),
                    static_cast<unsigned>(WiFi.softAPgetStationNum()));
    }
    if (apStarted_) startMdns();
    // A deferred request has not started a new driver scan yet.  Do not let
    // WiFi.scanComplete() report a stale result from the previous attempt.
    if (!scanDeferred_) refreshScanState();
    refreshStaState();
    emitStaIpObservation(now);
    if (scanDeferred_ && static_cast<int32_t>(now - scanNotBeforeMs_) >= 0) {
      const wl_status_t status = WiFi.status();
      if (!stationConfigured_ || (status != WL_IDLE_STATUS)) {
        scanDeferred_ = false;
        beginScan();
      }
    }
    if (scanPauseReconnect_) return;
    if (!stationConfigured_ || suppressed_ || WiFi.status() == WL_CONNECTED) return;
    if (static_cast<uint32_t>(now - lastAttemptMs_) < 5000U) return;
    ++reconnectAttempts_;
    lastAttemptMs_ = now;
    staState_ = StaState::Connecting;
    staFailure_ = StaFailure::None;
    WiFi.setAutoReconnect(true);
    (void)WiFi.reconnect();
  }

  bool connect(const char* ssid, const char* password) {
    if (!ssid || !*ssid || strlen(ssid) > 32 || !password || strlen(password) > 64) return false;
    WiFi.persistent(true);
    WiFi.mode(WIFI_AP_STA);
    WiFi.setAutoReconnect(true);
    suppressed_ = false;
    // WiFi.begin() starts an asynchronous association.  The driver may not
    // expose WiFi.SSID() until that work has progressed, so an accepted
    // configuration request must establish the runtime configured state from
    // the validated request itself.
    stationConfigured_ = true;
    staState_ = StaState::Connecting;
    staFailure_ = StaFailure::None;
    const wl_status_t result = WiFi.begin(ssid, password);
    Serial.printf("[WIFI] STA_BEGIN result=%d status=%d\n",
                  static_cast<int>(result), static_cast<int>(WiFi.status()));
    lastAttemptMs_ = millis();
    startMdns();
    (void)result;
    return stationConfigured_;
  }

  // Reconnect using only the driver's already-persisted STA configuration.
  // This path deliberately has no SSID/password parameters and never calls
  // WiFi.begin(), so it cannot overwrite the stored credentials.
  bool reconnect() {
    if (!(WiFi.getMode() & WIFI_MODE_STA) || !stationConfigured_) return false;
    WiFi.setAutoReconnect(true);
    suppressed_ = false;
    staState_ = StaState::Connecting;
    staFailure_ = StaFailure::None;
    lastAttemptMs_ = millis();
    (void)WiFi.reconnect();
    return true;
  }

  bool disconnect() {
    if (!(WiFi.getMode() & WIFI_MODE_STA)) return false;
    WiFi.setAutoReconnect(false);
    const bool ok = WiFi.disconnect(false, false);
    if (ok) {
      suppressed_ = true;
      staState_ = StaState::Disconnected;
      staFailure_ = StaFailure::None;
    }
    return ok;
  }

  bool forget() {
    if (!(WiFi.getMode() & WIFI_MODE_STA)) return false;
    WiFi.setAutoReconnect(false);
    const bool disconnected = WiFi.disconnect(false, true);
    if (!disconnected) return false;
    suppressed_ = true;
    stationConfigured_ = WiFi.SSID().length() != 0;
    staState_ = stationConfigured_ ? StaState::Disconnected : StaState::Unconfigured;
    staFailure_ = StaFailure::None;
    return !stationConfigured_;
  }

  bool startScan() {
    ++scanRequests_;
    Serial.printf("[WIFI] SCAN_REQUEST received count=%lu\n", static_cast<unsigned long>(scanRequests_));
    if (scanState_ == ScanState::Running) {
      Serial.println("[WIFI] SCAN_ALREADY_RUNNING accepted=true");
      return true;
    }
    wifi_mode_t mode = WIFI_MODE_NULL;
    const esp_err_t modeResult = esp_wifi_get_mode(&mode);
    const wl_status_t staStatus = WiFi.status();
    Serial.printf("[WIFI] SCAN_START_PREP mode=%d mode_result=%d sta_status=%d scan_before=%d ap_started=%s\n",
                  static_cast<int>(mode), static_cast<int>(modeResult),
                  static_cast<int>(staStatus), static_cast<int>(WiFi.scanComplete()),
                  apStarted_ ? "true" : "false");
    scanState_ = ScanState::Running;
    const bool staDriverBusy = stationConfigured_ &&
      (staStatus == WL_DISCONNECTED || staStatus == WL_IDLE_STATUS ||
       staStatus == WL_NO_SSID_AVAIL);
    if (staDriverBusy) {
      scanDeferred_ = true;
      scanPauseReconnect_ = true;
      WiFi.setAutoReconnect(false);
      (void)WiFi.disconnect(false, false);
      scanNotBeforeMs_ = millis() + 100U;
      Serial.printf("[WIFI] SCAN_DEFERRED reason=STA_DRIVER_BUSY status=%d\n",
                    static_cast<int>(staStatus));
      return true;
    }
    return beginScan();
  }

private:
  bool beginScan() {
    const int16_t result = WiFi.scanNetworks(true, true);
    scanLastResult_ = result;
    if (result == WIFI_SCAN_RUNNING) {
      ++scanAccepted_;
      Serial.printf("[WIFI] SCAN_ACCEPTED state=RUNNING result=%d accepted=%lu\n",
                    static_cast<int>(result), static_cast<unsigned long>(scanAccepted_));
      return true;
    }
    if (result >= 0) {
      scanState_ = ScanState::Complete;
      ++scanAccepted_;
      ++scanCompleted_;
      finishScan();
      Serial.printf("[WIFI] SCAN_COMPLETE count=%d completed=%lu\n",
                    static_cast<int>(result), static_cast<unsigned long>(scanCompleted_));
      return true;
    }
    scanState_ = ScanState::Unavailable;
    ++scanUnavailable_;
    finishScan();
    Serial.printf("[WIFI] SCAN_UNAVAILABLE result=%d failures=%lu\n",
                  static_cast<int>(result), static_cast<unsigned long>(scanUnavailable_));
    return false;
  }

  void finishScan() {
    scanDeferred_ = false;
    if (!scanPauseReconnect_) return;
    scanPauseReconnect_ = false;
    WiFi.setAutoReconnect(true);
    lastAttemptMs_ = millis();
  }

  void installEventDiagnostics() {
    if (eventHandlerRegistered_) return;
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
      switch (event) {
        case ARDUINO_EVENT_WIFI_STA_START:
          Serial.println("[WIFI] STA_EVENT event=START");
          break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
          Serial.printf("[WIFI] STA_EVENT event=CONNECTED channel=%u authmode=%u\n",
                        static_cast<unsigned>(info.wifi_sta_connected.channel),
                        static_cast<unsigned>(info.wifi_sta_connected.authmode));
          break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
          const uint8_t reason = info.wifi_sta_disconnected.reason;
          Serial.printf("[WIFI] STA_EVENT event=DISCONNECTED reason=%u name=%s\n",
                        static_cast<unsigned>(reason),
                        WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(reason)));
          // A driver disconnect while an association is in progress is a
          // terminal attempt failure until the bounded reconnect tick starts
          // another attempt.  Preserve deliberate Disconnect/Forget state.
          if (!suppressed_) {
            if (staFailure_ == StaFailure::None) {
              staFailure_ = staState_ == StaState::Connecting
                ? StaFailure::ConnectFailed
                : StaFailure::ConnectionLost;
            }
            staState_ = StaState::Disconnected;
          }
          break;
        }
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
          Serial.printf("[WIFI] STA_EVENT event=GOT_IP ip=%u.%u.%u.%u\n",
                        static_cast<unsigned>(info.got_ip.ip_info.ip.addr & 0xffU),
                        static_cast<unsigned>((info.got_ip.ip_info.ip.addr >> 8U) & 0xffU),
                        static_cast<unsigned>((info.got_ip.ip_info.ip.addr >> 16U) & 0xffU),
                        static_cast<unsigned>((info.got_ip.ip_info.ip.addr >> 24U) & 0xffU));
          break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
          Serial.println("[WIFI] STA_EVENT event=LOST_IP");
          break;
        default:
          break;
      }
    });
    eventHandlerRegistered_ = true;
  }

public:

  void status(Status& value) const {
    const bool connected = WiFi.status() == WL_CONNECTED;
    value.apStarted = apStarted_;
    value.staConfigured = stationConfigured_;
    value.staConnected = connected;
    value.reconnectSuppressed = suppressed_;
    value.mdnsStarted = mdnsStarted_;
    value.scanCount = WiFi.scanComplete();
    value.scanRunning = value.scanCount == WIFI_SCAN_RUNNING;
    value.scanState = scanState_;
    value.scanRequests = scanRequests_;
    value.scanAccepted = scanAccepted_;
    value.scanCompleted = scanCompleted_;
    value.scanUnavailable = scanUnavailable_;
    value.scanLastResult = scanLastResult_;
    value.apClients = WiFi.softAPgetStationNum();
    value.staRssi = value.staConnected ? WiFi.RSSI() : 0;
    value.reconnectAttempts = reconnectAttempts_;
    value.staState = connected ? StaState::Connected : staState_;
    value.staFailure = staFailure_;
    value.apIp = WiFi.softAPIP().toString();
    value.staIp = WiFi.localIP().toString();
    value.staSsid = stationConfigured_ ? WiFi.SSID() : String();
  }

private:
  // WiFi.SSID() reports the currently associated AP only.  The driver-owned
  // configuration is the authoritative persistent store and is available
  // through esp_wifi_get_config before association has begun.
  static bool hasStoredStationConfiguration() {
    wifi_config_t config{};
    if (esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_STA, &config) != ESP_OK) return false;
    return config.sta.ssid[0] != '\0';
  }

  void refreshStaState() {
    if (!stationConfigured_) {
      staState_ = StaState::Unconfigured;
      staFailure_ = StaFailure::None;
      return;
    }
    switch (WiFi.status()) {
      case WL_CONNECTED:
        staState_ = StaState::Connected;
        staFailure_ = StaFailure::None;
        break;
      case WL_NO_SSID_AVAIL:
        staState_ = StaState::Disconnected;
        staFailure_ = StaFailure::NoSsidAvailable;
        break;
      case WL_CONNECT_FAILED:
        staState_ = StaState::Disconnected;
        staFailure_ = StaFailure::ConnectFailed;
        break;
      case WL_CONNECTION_LOST:
        staState_ = StaState::Disconnected;
        staFailure_ = StaFailure::ConnectionLost;
        break;
      case WL_DISCONNECTED:
        staState_ = suppressed_ ? StaState::Disconnected :
                                 (staFailure_ == StaFailure::None ? StaState::Connecting : StaState::Disconnected);
        break;
      default:
        staState_ = StaState::Connecting;
        break;
    }
  }

  void refreshScanState() {
    if (scanState_ != ScanState::Running) return;
    const int16_t result = WiFi.scanComplete();
    if (result == WIFI_SCAN_RUNNING) return;
    scanLastResult_ = result;
    if (result >= 0) {
      scanState_ = ScanState::Complete;
      ++scanCompleted_;
      finishScan();
      Serial.printf("[WIFI] SCAN_COMPLETE count=%d completed=%lu\n",
                    static_cast<int>(result), static_cast<unsigned long>(scanCompleted_));
      return;
    }
    scanState_ = ScanState::Unavailable;
    ++scanUnavailable_;
    finishScan();
    Serial.printf("[WIFI] SCAN_UNAVAILABLE result=%d failures=%lu\n",
                  static_cast<int>(result), static_cast<unsigned long>(scanUnavailable_));
  }

  // Observation-only checkpoint for diagnosing a disappearing LAN address.
  // Event callbacks identify transitions, while this bounded periodic sample
  // also exposes a lost/zero address if the driver fails to emit an expected
  // event.  No credential or SSID material is included.
  void emitStaIpObservation(uint32_t now) {
    if (static_cast<uint32_t>(now - lastStaObservationMs_) < 5000U) return;
    lastStaObservationMs_ = now;
    const IPAddress staIp = WiFi.localIP();
    const IPAddress gateway = WiFi.gatewayIP();
    const IPAddress subnet = WiFi.subnetMask();
    const IPAddress apIp = WiFi.softAPIP();
    const bool staConnected = WiFi.status() == WL_CONNECTED;
    Serial.printf("[WIFI] STA_IP_OBSERVE ms=%lu driver_status=%d mode=%d configured=%u state=%s failure=%s connected=%u sta_ip=%s gateway=%s subnet=%s rssi=%ld suppressed=%u reconnect_attempts=%lu ap_started=%u ap_ip=%s ap_clients=%u mdns=%u\n",
                  static_cast<unsigned long>(now), static_cast<int>(WiFi.status()),
                  static_cast<int>(WiFi.getMode()), stationConfigured_ ? 1U : 0U,
                  staStateName(staState_), staFailureName(staFailure_),
                  staConnected ? 1U : 0U, staIp.toString().c_str(),
                  gateway.toString().c_str(), subnet.toString().c_str(),
                  staConnected ? static_cast<long>(WiFi.RSSI()) : 0L,
                  suppressed_ ? 1U : 0U,
                  static_cast<unsigned long>(reconnectAttempts_),
                  apStarted_ ? 1U : 0U, apIp.toString().c_str(),
                  static_cast<unsigned>(WiFi.softAPgetStationNum()),
                  mdnsStarted_ ? 1U : 0U);
  }

  ScanState scanState_ = ScanState::Idle;
  bool scanDeferred_ = false;
  bool scanPauseReconnect_ = false;
  bool eventHandlerRegistered_ = false;
  uint32_t scanNotBeforeMs_ = 0;
  uint32_t scanRequests_ = 0;
  uint32_t scanAccepted_ = 0;
  uint32_t scanCompleted_ = 0;
  uint32_t scanUnavailable_ = 0;
  int16_t scanLastResult_ = -2;

  void startMdns() {
    if (mdnsStarted_) return;
    if (!apStarted_ && WiFi.status() != WL_CONNECTED) return;
    if (MDNS.begin("plonk-and-play")) {
      MDNS.addService("http", "tcp", 80);
      mdnsStarted_ = true;
      Serial.printf("[WIFI] mDNS STARTED host=%s\n", MdnsHost);
    }
  }

  bool apStarted_ = false;
  bool stationConfigured_ = false;
  bool suppressed_ = false;
  bool mdnsStarted_ = false;
  uint32_t lastApAttemptMs_ = 0;
  uint32_t lastAttemptMs_ = 0;
  uint32_t lastStaObservationMs_ = 0;
  uint32_t reconnectAttempts_ = 0;
  StaState staState_ = StaState::Unconfigured;
  StaFailure staFailure_ = StaFailure::None;
};

} // namespace pp
