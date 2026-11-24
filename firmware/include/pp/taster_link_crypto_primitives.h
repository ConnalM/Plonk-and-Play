#pragma once
#include "taster_link_handshake.h"

namespace pp::taster_link::handshake {

// The reviewed PPTH protocol requests exactly one SHA256 output block.
// RFC5869 extract = HMAC(salt, IKM), expand = HMAC(PRK, info || 0x01).
// Keep this narrowly scoped to PPTH rather than enabling a new SDK feature
// or implementing a general-purpose KDF. HMAC and zeroisation are supplied
// by the platform crypto library. No raw PSK is a record key.
template<class Hmac,class Wipe>
bool ppthHkdf32(const Secret& psk,const uint8_t* salt,size_t saltSize,
               const uint8_t* info,size_t infoSize,uint8_t* out,size_t outSize,
               Hmac hmac,Wipe wipe) {
  constexpr size_t MaxInfo=64;
  if(!out || outSize!=Secret{}.size()) return false;
  if(!salt || saltSize!=TranscriptSize || !info || !infoSize || infoSize>MaxInfo) {
    wipe(out,outSize);return false;
  }
  Secret prk{},derived{};
  std::array<uint8_t,MaxInfo+1> block{};
  std::memcpy(block.data(),info,infoSize);
  block[infoSize]=1;
  const bool ok=hmac(salt,saltSize,psk.data(),psk.size(),prk.data()) &&
    hmac(prk.data(),prk.size(),block.data(),infoSize+1,derived.data());
  if(ok) std::memcpy(out,derived.data(),derived.size());
  else wipe(out,outSize);
  wipe(prk.data(),prk.size());wipe(derived.data(),derived.size());
  return ok;
}

// Allocate once per actual Core boot, after hardware entropy is available.
// Never recreate LinkIds on listener restart, AP address change or reconnect.
// The 64-bit random epoch is probabilistic uniqueness, not a persisted counter.
inline bool newBootEpoch(const Crypto& crypto,uint64_t& out) {
  out=0;
  std::array<uint8_t,8> bytes{};
  if(!crypto.random(bytes.data(),bytes.size())) {
    crypto.wipe(bytes.data(),bytes.size());return false;
  }
  const auto* p=bytes.data();
  const auto epoch=wire::get(p,8);
  crypto.wipe(bytes.data(),bytes.size());
  if(!epoch) return false;
  out=epoch;return true;
}

} // namespace pp::taster_link::handshake
