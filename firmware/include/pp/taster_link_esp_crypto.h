#pragma once
#if !defined(ESP_PLATFORM)
#error "The ESP provider requires the ESP-IDF runtime"
#endif
#include "taster_link_crypto_primitives.h"
#include "system_time.h"
#include "taster_link_transport.h"
#include <esp_random.h>
#include <mbedtls/md.h>
#include <mbedtls/platform_util.h>

namespace pp::taster_link::transport {

// The platform owner must certify Wi-Fi/RF entropy is actually enabled
// and keep it enabled across random(). Merely having a configured SSID,
// interface mode or saved credentials is not evidence of entropy.
// Fail closed before radio start/after stop; never fall back to millis,
// MAC address, esp_random without entropy, or a fixed secret.
class EspCrypto final : public handshake::Crypto {
public:
  using EntropyReady=bool (*)();
  explicit EspCrypto(EntropyReady ready):ready_(ready){}
  bool random(uint8_t* out,size_t length) const override {
    if(!out || !length || !ready_ || !ready_()) return false;
    esp_fill_random(out,length);
    if(!ready_()) {wipe(out,length);return false;}
    return true;
  }
  bool hkdf(const handshake::Secret& psk,const uint8_t* salt,size_t saltSize,
            const uint8_t* info,size_t infoSize,uint8_t* out,size_t outSize) const override {
    return handshake::ppthHkdf32(psk,salt,saltSize,info,infoSize,out,outSize,
      &rawHmac,&zero);
  }
  bool hmac(const handshake::Secret& key,const uint8_t* data,size_t length,
            handshake::Tag& out) const override {
    if(rawHmac(key.data(),key.size(),data,length,out.data())) return true;
    wipe(out.data(),out.size());return false;
  }
  bool equal(const uint8_t* a,const uint8_t* b,size_t n) const override {
    if (!a || !b) return false;
    // The recovered production SDK does not link mbedtls_ct_memcmp.  Keep
    // comparison timing independent of the first differing byte without
    // weakening authenticated-link verification or adding an SDK dependency.
    volatile uint8_t different = 0;
    for (size_t i = 0; i < n; ++i) different |= static_cast<uint8_t>(a[i] ^ b[i]);
    return different == 0;
  }
  void wipe(uint8_t* p,size_t n) const override {zero(p,n);}
private:
  static bool rawHmac(const uint8_t* key,size_t keySize,const uint8_t* data,size_t size,uint8_t* out){
    const auto* md=mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    return md && key && data && out && mbedtls_md_hmac(md,key,keySize,data,size,out)==0;
  }
  static void zero(uint8_t* p,size_t n){if(p && n) mbedtls_platform_zeroize(p,n);}
  const EntropyReady ready_;
};

// Use the existing Core monotonic service, not wall time or millis().
class EspClock final : public ClockSource {
public:
  uint64_t now() const override {return pp::systemTime();}
};

} // namespace pp::taster_link::transport
