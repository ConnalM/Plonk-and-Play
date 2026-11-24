#pragma once
#if !defined(ESP_PLATFORM) || !defined(ARDUINO)
#error "Taster discovery adapter requires ESP-IDF/Arduino"
#endif
#include "taster_link_discovery.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <nvs.h>
#include "taster_link_pairing_esp_store.h"

namespace pp::taster_link::discovery {

// Separate locator record in the already protected pairing partition.
// Contains no PSK; not an alternative credential store. Owner keeps the
// EspStore alive, initialises it securely first and serialises maintenance.
// Cross-record interruption is detected by matching the vault PairId.
class EspHintStore final:public HintStore {
public:
  explicit EspHintStore(const pairing::EspStore& protectedStore):store_(protectedStore){}
  Read read(HintRecord& out) override {
    out={};if(!store_.protectedReady()) return Read::Error;
    nvs_handle_t handle{};
    auto err=nvs_open_from_partition(pairing::EspStore::Partition,
      pairing::EspStore::Namespace,NVS_READONLY,&handle);
    if(err==ESP_ERR_NVS_NOT_FOUND) return Read::Missing;
    if(err!=ESP_OK) return Read::Error;
    size_t n=out.size();err=nvs_get_blob(handle,Key,out.data(),&n);nvs_close(handle);
    if(err==ESP_ERR_NVS_NOT_FOUND) return Read::Missing;
    return err==ESP_OK && n==out.size()?Read::Found:Read::Error;
  }
  bool replace(const HintRecord& record) override {
    if(!store_.protectedReady()) return false;
    nvs_handle_t handle{};
    if(nvs_open_from_partition(pairing::EspStore::Partition,
       pairing::EspStore::Namespace,NVS_READWRITE,&handle)!=ESP_OK) return false;
    const bool ok=nvs_set_blob(handle,Key,record.data(),record.size())==ESP_OK &&
      nvs_commit(handle)==ESP_OK;
    nvs_close(handle);return ok;
  }
private:
  static constexpr const char* Key="core_hint";
  const pairing::EspStore& store_;
};

// Taster-only, exclusive Wi-Fi owner source adapter. Never instantiate on
// Core or integrated firmware; no scan or association on Core's AP/STA
// manager. The future platform owner manages network generation, scan
// retry cadence, connection deadline, transport stop, and credential
// bootstrap. No call from an asynchronous Wi-Fi event callback.
class EspDiscovery {
public:
  static constexpr uint64_t ScanTimeoutUs=10000000;
  static constexpr int MaxScanRows=64;
  explicit EspDiscovery(Selector& selector):selector_(selector){}
  EspDiscovery(const EspDiscovery&)=delete;
  EspDiscovery& operator=(const EspDiscovery&)=delete;
  // Caller has selected exclusive WIFI_STA, disabled driver reconnect,
  // stopped and destroyed any old transport and disconnected old AP.
  bool beginScan(uint64_t now,bool quiescent){
    if(fault_ || token_ || !quiescent || WiFi.getMode()!=WIFI_STA ||
       WiFi.status()==WL_CONNECTED) return false;
    if(WiFi.scanComplete()==WIFI_SCAN_RUNNING) return false;
    const auto token=selector_.beginScan(now);if(!token) return false;
    WiFi.scanDelete();token_=token;started_=now;
    const int result=WiFi.scanNetworks(true,false);
    if(result==WIFI_SCAN_RUNNING) return true;
    finish(result,now);return result>=0;
  }
  void pollScan(uint64_t now,bool quiescent){
    if(!token_) return;
    if(!quiescent || WiFi.getMode()!=WIFI_STA || WiFi.status()==WL_CONNECTED ||
       now<started_ || now-started_>=ScanTimeoutUs) {
      // Never adopt a delayed result from an aborted scan. Require owner
      // recovery/reinitialisation after cancellation, not stale retry.
      (void)esp_wifi_scan_stop();WiFi.scanDelete();fault_=true;
      selector_.finishScan(token_,false,now);token_=0;return;
    }
    const int result=WiFi.scanComplete();
    if(result!=WIFI_SCAN_RUNNING) finish(result,now);
  }
  bool faulted() const{return fault_;}
  // An association request is not a successful connection. Pin to the
  // selected BSSID and channel; no driver-managed arbitrary AP fallback.
  // The selected AP uses the existing open Core network. PPTH/bootstrap
  // is a separate boundary; no credentials inferred from this operation.
  bool associate(uint64_t attemptToken,bool quiescent){
    const auto& a=selector_.attempt();
    if(fault_ || token_ || !quiescent || WiFi.getMode()!=WIFI_STA ||
       WiFi.status()==WL_CONNECTED || selector_.state()!=State::Connecting ||
       !a.valid() || a.token!=attemptToken || associationToken_==attemptToken) return false;
    associationToken_=attemptToken;
    WiFi.persistent(false);WiFi.setAutoReconnect(false);
    (void)WiFi.begin(CoreSsid,nullptr,a.candidate.channel,a.candidate.bssid.data(),true);
    return true;
  }
  // Verify live association, not a cached scan result. Owner must also
  // establish the expected local address/Core endpoint/network epoch.
  bool associated(uint64_t attemptToken) const {
    const auto& a=selector_.attempt();
    if(fault_ || !attemptToken || associationToken_!=attemptToken ||
       !a.valid() || a.token!=attemptToken || WiFi.getMode()!=WIFI_STA ||
       WiFi.status()!=WL_CONNECTED) return false;
    wifi_ap_record_t ap{};
    return esp_wifi_sta_get_ap_info(&ap)==ESP_OK &&
      std::memcmp(ap.bssid,a.candidate.bssid.data(),6)==0 &&
      std::memcmp(ap.ssid,CoreSsid,sizeof(CoreSsid))==0;
  }
private:
  void finish(int rows,uint64_t now){
    bool complete=rows>=0 && rows<=MaxScanRows;
    if(complete) for(int i=0;i<rows;++i){
      if(WiFi.SSID(i)!=CoreSsid) continue;
      const auto* b=WiFi.BSSID(i);const int ch=WiFi.channel(i);
      if(!b || ch<1 || ch>14) {complete=false;break;}
      Candidate c{};std::memcpy(c.bssid.data(),b,6);
      c.channel=static_cast<uint8_t>(ch);
      const auto signal=WiFi.RSSI(i);
      if(signal<INT16_MIN || signal>INT16_MAX){complete=false;break;}
      c.rssi=static_cast<int16_t>(signal);
      if(!selector_.observe(token_,c)){complete=false;break;}
    }
    WiFi.scanDelete();selector_.finishScan(token_,complete,now);token_=0;
  }
  Selector& selector_;
  uint64_t token_=0,started_=0,associationToken_=0;
  bool fault_=false;
};

} // namespace pp::taster_link::discovery
