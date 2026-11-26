#pragma once
#if !defined(ESP_PLATFORM) || !defined(ARDUINO)
#error "Role startup requires ESP-IDF/Arduino"
#endif
#include "s4_production_role_guard.h"
#include "taster_link_role_contract.h"
#include "taster_link_pairing.h"
#include "taster_link_esp_crypto.h"
#if defined(MBEDTLS_USE_PSA_CRYPTO)
#include <psa/crypto.h>
#endif

namespace pp::taster_link::startup {
using Approval = role_contract::Approval;
using State = role_contract::StartupState;
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
using RoleEventSlot = role_contract::EventSlot;
} // namespace pp::taster_link::startup
