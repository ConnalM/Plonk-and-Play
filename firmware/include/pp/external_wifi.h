#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>

namespace pp {

// The Arduino/ESP-IDF STA configuration is the sole persistent credential
// store.  This manager keeps only non-secret runtime state and never reads or
// emits the STA password.
class ExternalWifiManager {
public:
  static constexpr const char* FactoryApSsid = "Plonk-and-Play";
  static constexpr const char* MdnsHost = "plonk-and-play.local";

  struct Status {
    bool apStarted = false;
    bool staConfigured = false;
    bool staConnected = false;
    bool reconnectSuppressed = false;
    bool mdnsStarted = false;
    bool scanRunning = false;
    int16_t scanCount = -2;
    uint8_t apClients = 0;
    int32_t staRssi = 0;
    uint32_t reconnectAttempts = 0;
    String apIp;
    String staIp;
    String staSsid;
  };

  void begin() {
    WiFi.persistent(true);
    WiFi.mode(WIFI_AP_STA);
    apStarted_ = WiFi.softAP(FactoryApSsid);
    suppressed_ = false;
    stationConfigured_ = WiFi.SSID().length() != 0;
    if (stationConfigured_) {
      WiFi.setAutoReconnect(true);
      (void)WiFi.begin();
      lastAttemptMs_ = millis();
    }
    startMdns();
    Serial.printf("[WIFI] P&P AP %s ssid=%s ip=%s clients=%u\n",
                  apStarted_ ? "STARTED" : "FAILED", FactoryApSsid,
                  WiFi.softAPIP().toString().c_str(),
                  static_cast<unsigned>(WiFi.softAPgetStationNum()));
    if (stationConfigured_) {
      Serial.printf("[WIFI] external STA remembered ssid=%s\n", WiFi.SSID().c_str());
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
    if (!stationConfigured_ || suppressed_ || WiFi.status() == WL_CONNECTED) return;
    if (static_cast<uint32_t>(now - lastAttemptMs_) < 5000U) return;
    ++reconnectAttempts_;
    lastAttemptMs_ = now;
    WiFi.setAutoReconnect(true);
    (void)WiFi.reconnect();
  }

  bool connect(const char* ssid, const char* password) {
    if (!ssid || !*ssid || strlen(ssid) > 32 || !password || strlen(password) > 64) return false;
    WiFi.persistent(true);
    WiFi.mode(WIFI_AP_STA);
    WiFi.setAutoReconnect(true);
    suppressed_ = false;
    const wl_status_t result = WiFi.begin(ssid, password);
    stationConfigured_ = WiFi.SSID().length() != 0;
    lastAttemptMs_ = millis();
    startMdns();
    (void)result;
    return stationConfigured_;
  }

  bool disconnect() {
    if (!(WiFi.getMode() & WIFI_MODE_STA)) return false;
    WiFi.setAutoReconnect(false);
    const bool ok = WiFi.disconnect(false, false);
    if (ok) suppressed_ = true;
    return ok;
  }

  bool forget() {
    if (!(WiFi.getMode() & WIFI_MODE_STA)) return false;
    WiFi.setAutoReconnect(false);
    const bool disconnected = WiFi.disconnect(false, true);
    if (!disconnected) return false;
    suppressed_ = true;
    stationConfigured_ = WiFi.SSID().length() != 0;
    return !stationConfigured_;
  }

  bool startScan() {
    const int16_t result = WiFi.scanNetworks(true, true);
    return result == WIFI_SCAN_RUNNING || result >= 0;
  }

  void status(Status& value) const {
    value.apStarted = apStarted_;
    value.staConfigured = stationConfigured_;
    value.staConnected = WiFi.status() == WL_CONNECTED;
    value.reconnectSuppressed = suppressed_;
    value.mdnsStarted = mdnsStarted_;
    value.scanCount = WiFi.scanComplete();
    value.scanRunning = value.scanCount == WIFI_SCAN_RUNNING;
    value.apClients = WiFi.softAPgetStationNum();
    value.staRssi = value.staConnected ? WiFi.RSSI() : 0;
    value.reconnectAttempts = reconnectAttempts_;
    value.apIp = WiFi.softAPIP().toString();
    value.staIp = WiFi.localIP().toString();
    value.staSsid = stationConfigured_ ? WiFi.SSID() : String();
  }

private:
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
};

} // namespace pp
