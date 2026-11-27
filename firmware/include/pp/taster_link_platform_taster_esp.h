#pragma once
#if !defined(ESP_PLATFORM) || !defined(ARDUINO)
#error "Taster connection platform requires ESP-IDF/Arduino"
#endif
#include "taster_link_platform.h"
#include "taster_link_lifecycle.h"
#include "taster_link_discovery_esp.h"
#include <esp_netif.h>
#include <esp_wifi.h>

namespace pp::taster_link::platform {
namespace lc=lifecycle;
namespace ds=discovery;

inline bool staEvent(WiFiEvent_t e){
  return e==ARDUINO_EVENT_WIFI_STA_START || e==ARDUINO_EVENT_WIFI_STA_STOP ||
    e==ARDUINO_EVENT_WIFI_STA_CONNECTED || e==ARDUINO_EVENT_WIFI_STA_DISCONNECTED ||
    e==ARDUINO_EVENT_WIFI_STA_GOT_IP || e==ARDUINO_EVENT_WIFI_STA_LOST_IP;
}
inline sr::Network tasterInterfaceSample(const char* key){
  sr::Network n{};
  auto* netif=esp_netif_get_handle_from_ifkey(key);
  if(!netif || !esp_netif_is_netif_up(netif))return {};
  esp_netif_ip_info_t ip{};
  if(esp_netif_get_ip_info(netif,&ip)!=ESP_OK ||
     esp_netif_get_netif_impl_name(netif,n.interfaceName.data())!=ESP_OK ||
     !n.interfaceName[0] || n.interfaceName.back()!=0)return {};
  n.local=ip.ip.addr;n.core=ip.gw.addr;
  if(!sr::usableAddress(n.local) || !sr::usableAddress(n.core) || n.local==n.core ||
     !ip.netmask.addr || (n.local&ip.netmask.addr)!=(n.core&ip.netmask.addr))return {};
  n.ready=true;return n;
}

// Exclusive Taster-only STA. Never instantiate on Core or integrated firmware.
class EspRadio final:public lc::Radio {
public:
  void noteEvent(WiFiEvent_t e){if(staEvent(e))events_.changed();}
  bool beginScan(ds::Selector& selector,uint64_t now) override{
    if(fault_ || scan_ || WiFi.getMode()!=WIFI_OFF)return false;
    WiFi.persistent(false);WiFi.setAutoReconnect(false);
    if(!WiFi.mode(WIFI_STA) || esp_wifi_set_storage(WIFI_STORAGE_RAM)!=ESP_OK ||
       WiFi.getMode()!=WIFI_STA || WiFi.status()==WL_CONNECTED ||
       WiFi.scanComplete()==WIFI_SCAN_RUNNING)return fault();
    WiFi.setAutoReconnect(false);
    scan_.emplace(selector);selector_=&selector;
    if(!scan_->beginScan(now,true))return fault();
    return true;
  }
  bool pollScan(ds::Selector& selector,uint64_t now) override{
    if(fault_ || !scan_ || selector_!=&selector)return false;
    scan_->pollScan(now,true);return !scan_->faulted();
  }
  bool associate(const ds::Attempt& a) override{
    if(fault_ || !scan_ || !selector_ || !a.valid() ||
       selector_->attempt().token!=a.token ||
       selector_->attempt().candidate.bssid!=a.candidate.bssid)return false;
    epoch_.invalidate();return scan_->associate(a.token,true);
  }
  sr::Network associated(const ds::Attempt& a) const override{
    const auto before=events_.value();sr::Network n{};
    if(!fault_ && scan_ && selector_ && selector_->attempt().token==a.token &&
       selector_->attempt().candidate.bssid==a.candidate.bssid && scan_->associated(a.token)){
      n=tasterInterfaceSample("WIFI_STA_DEF");
      if(!scan_->associated(a.token))n={};
    }
    return epoch_.observe(n,before,events_.value());
  }
  bool stop() override{
    const auto mode=WiFi.getMode();if(mode!=WIFI_OFF && mode!=WIFI_STA)return fault();
    const bool interrupted=mode==WIFI_STA && WiFi.scanComplete()==WIFI_SCAN_RUNNING;
    if(interrupted)(void)esp_wifi_scan_stop();
    if(mode==WIFI_STA){
      WiFi.setAutoReconnect(false);WiFi.scanDelete();
      (void)WiFi.disconnect(false,false);if(!WiFi.mode(WIFI_OFF))fault_=true;
    }
    scan_.reset();selector_=nullptr;epoch_.invalidate();
    if(WiFi.getMode()!=WIFI_OFF || interrupted) {
      fault_=true;
    }
    return !fault_;
  }
private:
  bool fault(){fault_=true;return false;}
  EventRevision events_;mutable NetworkEpoch epoch_;
  std::optional<ds::EspDiscovery> scan_;ds::Selector* selector_=nullptr;
  bool fault_=false;
};
} // namespace pp::taster_link::platform
