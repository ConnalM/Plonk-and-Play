#pragma once
#if !defined(ESP_PLATFORM)
#error "Pairing NVS backend requires ESP-IDF"
#endif
#include "taster_link_pairing.h"
#include <nvs.h>
#include <nvs_flash.h>

namespace pp::taster_link::pairing {

// Opt-in dedicated encrypted NVS partition. No partition table or security
// key configuration is changed here. Board provisioning must supply a
// protected device-specific nvs_sec_cfg_t via the approved key lifecycle,
// and ensure this partition is exclusively owned and NOT pre-initialised
// as plaintext. Secure init is not key generation or a security audit.
// No automatic init of default NVS, erase, key generation or fallback.
class EspStore final : public Store {
public:
  static constexpr const char* Partition="pp_pair";
  static constexpr const char* Namespace="pairing";
  static constexpr const char* Key="record";
  EspStore()=default;
  EspStore(const EspStore&)=delete;
  EspStore& operator=(const EspStore&)=delete;
  ~EspStore(){
    if(ready_) (void)nvs_flash_deinit_partition(Partition);
  }
  // Exclusive startup only; caller owns and wipes cfg and verifies key
  // protection. Never initialise/retry in a race or an HTTP callback.
  bool begin(nvs_sec_cfg_t& cfg,bool keyProtectionApproved) {
    if(attempted_) return ready_;
    attempted_=true;
    if(!keyProtectionApproved) return false;
    ready_=nvs_flash_secure_init_partition(Partition,&cfg)==ESP_OK;
    return ready_;
  }
  bool protectedReady() const override {return ready_;}
  Read read(Record& out) override {
    out={};
    if(!ready_) return Read::Error;
    nvs_handle_t handle{};
    auto err=nvs_open_from_partition(Partition,Namespace,NVS_READONLY,&handle);
    if(err==ESP_ERR_NVS_NOT_FOUND) return Read::Missing;
    if(err!=ESP_OK) return Read::Error;
    size_t size=out.size();
    err=nvs_get_blob(handle,Key,out.data(),&size);
    nvs_close(handle);
    if(err==ESP_ERR_NVS_NOT_FOUND) return Read::Missing;
    return err==ESP_OK && size==out.size()?Read::Found:Read::Error;
  }
  bool replace(const Record& record) override {
    if(!ready_) return false;
    nvs_handle_t handle{};
    if(nvs_open_from_partition(Partition,Namespace,NVS_READWRITE,&handle)!=ESP_OK) return false;
    const bool ok=nvs_set_blob(handle,Key,record.data(),record.size())==ESP_OK &&
      nvs_commit(handle)==ESP_OK;
    nvs_close(handle);
    return ok;
  }
private:
  bool attempted_=false,ready_=false;
};

} // namespace pp::taster_link::pairing
