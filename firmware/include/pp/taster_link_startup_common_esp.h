#pragma once
#if !defined(ESP_PLATFORM) || !defined(ARDUINO)
#error "Role startup requires ESP-IDF/Arduino"
#endif
#include "taster_link_pairing.h"
#include "taster_link_esp_crypto.h"
#if defined(MBEDTLS_USE_PSA_CRYPTO)
#include <psa/crypto.h>
#endif

namespace pp::taster_link::startup {
// Only trusted local boot/owner code supplies these facts, NEVER network
// requests. Default-deny. Target qualification and protected device-key
// provisioning are prerequisites, not manufactured by this composition.
struct Approval {
  bool commissioned=false;
  bool targetQualified=false; // actual SDK, entropy, interface, timing/resources
  bool exclusiveOwnership=false;
  bool idleForBootstrap=false; // Core race/engine/storage idle; Taster exclusive STA
  bool enabled() const{return commissioned && targetQualified && exclusiveOwnership;}
};
enum class State:uint8_t {Cold,Waiting,Operating,Recovery,Fault};
inline bool initCrypto(){
#if defined(MBEDTLS_USE_PSA_CRYPTO)
  return psa_crypto_init()==PSA_SUCCESS;
#else
  return true;
#endif
}
inline bool usableVault(pairing::State s){
  return s==pairing::State::Empty || s==pairing::State::Paired;
}
// Exactly one independent role may install a Wi-Fi callback per boot.  The
// callback target has boot lifetime; no unregister/destruction or role
// switching at runtime. The compile-only probes reference this guard but never
// call either installer.
class RoleEventSlot {
public:
  static bool claim(){if(installed_)return false;installed_=true;return true;}
private:
  inline static bool installed_=false;
};
} // namespace pp::taster_link::startup
