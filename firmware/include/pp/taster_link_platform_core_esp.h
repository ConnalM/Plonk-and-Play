#pragma once
#if !defined(ESP_PLATFORM) || !defined(ARDUINO)
#error "Core connection platform requires ESP-IDF/Arduino"
#endif
// lwIP's INADDR_NONE collides with Arduino IPAddress.  This boundary must
// precede every project include: both the socket platform and RaceControl's
// storage chain can reach lwIP/Arduino transitively.
#include <lwip/inet.h>
#pragma push_macro("INADDR_NONE")
#undef INADDR_NONE
#include <Arduino.h>
#include <WiFi.h>
#pragma pop_macro("INADDR_NONE")
#include "taster_link_platform.h"
#include "taster_link_lifecycle.h"
#include "taster_link_discovery.h"
#include "race_control.h"
#include <esp_netif.h>
#include <esp_wifi.h>

namespace pp::taster_link::platform {
namespace lc=lifecycle;
namespace ds=discovery;

inline bool apEvent(WiFiEvent_t e){
  return e==ARDUINO_EVENT_WIFI_AP_START || e==ARDUINO_EVENT_WIFI_AP_STOP;
}
inline sr::Network interfaceSample(const char* key,bool coreRole){
  sr::Network n{};
  auto* netif=esp_netif_get_handle_from_ifkey(key);
  if(!netif || !esp_netif_is_netif_up(netif))return {};
  esp_netif_ip_info_t ip{};
  if(esp_netif_get_ip_info(netif,&ip)!=ESP_OK ||
     esp_netif_get_netif_impl_name(netif,n.interfaceName.data())!=ESP_OK ||
     !n.interfaceName[0] || n.interfaceName.back()!=0)return {};
  n.local=ip.ip.addr;n.core=coreRole?n.local:ip.gw.addr;
  if(!sr::usableAddress(n.local) || !sr::usableAddress(n.core) ||
     (!coreRole && (n.local==n.core || !ip.netmask.addr ||
      (n.local&ip.netmask.addr)!=(n.core&ip.netmask.addr))))return {};
  n.ready=true;return n;
}

// Read-only observation of Core's existing AP; never starts, stops or scans it.
class EspCoreNetwork {
public:
  void noteEvent(WiFiEvent_t e){if(apEvent(e))events_.changed();}
  sr::Network sample() const {
    const auto before=events_.value();sr::Network n{};
    wifi_mode_t mode{};wifi_config_t config{};
    if(esp_wifi_get_mode(&mode)==ESP_OK &&
       (mode==WIFI_MODE_AP || mode==WIFI_MODE_APSTA) &&
       esp_wifi_get_config(WIFI_IF_AP,&config)==ESP_OK &&
       config.ap.ssid_len==sizeof(ds::CoreSsid)-1 &&
       std::memcmp(config.ap.ssid,ds::CoreSsid,sizeof(ds::CoreSsid)-1)==0 &&
       config.ap.authmode==WIFI_AUTH_OPEN)
      n=interfaceSample("WIFI_AP_DEF",true);
    return epoch_.observe(n,before,events_.value());
  }
private:
  EventRevision events_;mutable NetworkEpoch epoch_;
};

// Local policy tied to actual Race Control, never to a remote request.
class EspCorePolicy final:public lc::CorePolicy,public pairing::Maintenance {
public:
  EspCorePolicy(const pp::RaceControlModule& race,const EspCoreNetwork& net)
    :race_(race),net_(net){}
  void prepare(const sr::Network& current,bool commissioned,bool protectedReady,
               bool resourceQualified,bool noBorrowers,bool ownerIdle){
    current_=current;quiet_=noBorrowers;ownerIdle_=ownerIdle;
    approved_=commissioned && protectedReady && resourceQualified;
  }
  bool locallyAuthorised() const override{return approved_;}
  bool safeLifecycle() const override{
    return ownerIdle_ && race_.state()==pp::SessionLifecycle::Ready &&
      race_.mode()==pp::SessionMode::None && race_.definition()==nullptr;
  }
  bool transportQuiescent() const override{return quiet_;}
  bool bootstrapSafe() const override{
    return locallyAuthorised() && safeLifecycle() && transportQuiescent() &&
      lc::coreNetwork(current_) && net_.sample().same(current_);
  }
  bool bootstrapPeer(int fd,const sr::Network& expected) const override{
    if(!bootstrapSafe() || !expected.same(current_) || fd<0 ||
       !expected.interfaceName[0])return false;
    // Listener was bound to this interface BEFORE listen. Target
    // qualification must verify lwIP inbound filtering and PCB inheritance;
    // local address checks alone do not establish interface provenance.
    sockaddr_in local{},peer{};socklen_t ln=sizeof(local),pn=sizeof(peer);
    return ::getsockname(fd,reinterpret_cast<sockaddr*>(&local),&ln)==0 &&
      ::getpeername(fd,reinterpret_cast<sockaddr*>(&peer),&pn)==0 &&
      ln==sizeof(local) && pn==sizeof(peer) && local.sin_family==AF_INET &&
      peer.sin_family==AF_INET && local.sin_addr.s_addr==expected.local &&
      sr::usableAddress(peer.sin_addr.s_addr) && peer.sin_addr.s_addr!=expected.local &&
      net_.sample().same(expected);
  }
private:
  const pp::RaceControlModule& race_;const EspCoreNetwork& net_;
  sr::Network current_{};bool approved_=false,quiet_=false,ownerIdle_=false;
};
} // namespace pp::taster_link::platform
