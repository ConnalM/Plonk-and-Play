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
    refreshScanState();
    refreshStaState();
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
    wifi_mode_t mode = WIFI_MODE_NULL;
    const esp_err_t modeResult = esp_wifi_get_mode(&mode);
    Serial.printf("[WIFI] SCAN_START_PREP mode=%d mode_result=%d sta_status=%d scan_before=%d ap_started=%s\n",
                  static_cast<int>(mode), static_cast<int>(modeResult),
                  static_cast<int>(WiFi.status()), static_cast<int>(WiFi.scanComplete()),
                  apStarted_ ? "true" : "false");
    const int16_t result = WiFi.scanNetworks(true, true);
    scanLastResult_ = result;
    if (result == WIFI_SCAN_RUNNING) {
      scanState_ = ScanState::Running;
      ++scanAccepted_;
      Serial.printf("[WIFI] SCAN_ACCEPTED state=RUNNING result=%d accepted=%lu\n",
                    static_cast<int>(result), static_cast<unsigned long>(scanAccepted_));
      return true;
    }
    if (result >= 0) {
      scanState_ = ScanState::Complete;
      ++scanAccepted_;
      ++scanCompleted_;
      Serial.printf("[WIFI] SCAN_COMPLETE count=%d completed=%lu\n",
                    static_cast<int>(result), static_cast<unsigned long>(scanCompleted_));
      return true;
    }
    scanState_ = ScanState::Unavailable;
    ++scanUnavailable_;
    Serial.printf("[WIFI] SCAN_UNAVAILABLE result=%d failures=%lu\n",
                  static_cast<int>(result), static_cast<unsigned long>(scanUnavailable_));
    return false;
  }

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
      Serial.printf("[WIFI] SCAN_COMPLETE count=%d completed=%lu\n",
                    static_cast<int>(result), static_cast<unsigned long>(scanCompleted_));
      return;
    }
    scanState_ = ScanState::Unavailable;
    ++scanUnavailable_;
    Serial.printf("[WIFI] SCAN_UNAVAILABLE result=%d failures=%lu\n",
                  static_cast<int>(result), static_cast<unsigned long>(scanUnavailable_));
  }

  ScanState scanState_ = ScanState::Idle;
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
  uint32_t reconnectAttempts_ = 0;
  StaState staState_ = StaState::Unconfigured;
  StaFailure staFailure_ = StaFailure::None;
};

} // namespace pp
