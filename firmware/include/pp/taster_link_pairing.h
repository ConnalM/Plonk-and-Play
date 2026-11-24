#pragma once
#include "taster_link_handshake.h"

namespace pp::taster_link::pairing {
namespace hs=handshake;

// Credential records are private to one device role. A matched pair has
// identical PairId/PSK but different local role records. Revision is local
// maintenance sequencing, NOT a cross-device counter or anti-rollback fuse.
enum class Role:uint8_t { Core=1,Taster=2 };
enum class State:uint8_t { Unloaded,Empty,Revoked,Paired,Fault };
enum class Read:uint8_t { Found,Missing,Error };
enum class Result:uint8_t { Applied,Denied,WrongState,Stale,Invalid,StorageFault };
constexpr size_t RecordSize=64;
using Record=std::array<uint8_t,RecordSize>;

// Backend must be exclusive, encrypted at rest, durable on successful write,
// and atomically replace ONE complete record (not independently updated
// key/role/revision fields). A failed write has uncertain persistence.
// No erase/reinitialise/old-slot recovery on read or write failure.
class Store {
public:
  virtual ~Store()=default;
  virtual bool protectedReady() const=0;
  virtual Read read(Record&)=0;
  virtual bool replace(const Record&)=0;
};

// Trusted physical/service commissioning owner supplies these facts at the
// maintenance execution boundary. They are NOT wire/HTTP input fields.
// A stopped service must have destroyed every Peer and released every
// credential reference; no new borrower may start during maintenance.
class Maintenance {
public:
  virtual ~Maintenance()=default;
  virtual bool locallyAuthorised() const=0;
  virtual bool safeLifecycle() const=0; // Core READY/no active race; Taster service mode
  virtual bool transportQuiescent() const=0;
};
inline bool validRole(Role role){return role==Role::Core || role==Role::Taster;}
inline uint32_t checksum(const uint8_t* p,size_t n) {
  // Accidental corruption detection only, NOT cryptographic integrity.
  uint32_t crc=UINT32_MAX;
  while(n--) {
    crc^=*p++;
    for(unsigned i=0;i<8;++i) crc=(crc>>1)^((crc&1)?0xEDB88320U:0);
  }
  return ~crc;
}
class Material {
public:
  explicit Material(const hs::Crypto& crypto):crypto_(crypto){}
  ~Material(){clear();}
  Material(const Material&)=delete;
  Material& operator=(const Material&)=delete;
  const hs::PairId& pair() const {return pair_;}
  const hs::Secret& psk() const {return psk_;}
  bool valid() const {return hs::nonzero(pair_) && hs::nonzero(psk_);}
  void clear(){crypto_.wipe(pair_.data(),pair_.size());crypto_.wipe(psk_.data(),psk_.size());}
  // Only a trusted local provisioning channel may supply material; never
  // a short PIN, device address, SSID, user password or Internet command.
  bool import(const hs::PairId& pair,const hs::Secret& key) {
    if(&pair==&pair_ || &key==&psk_) return false;
    clear();
    if(!hs::nonzero(pair) || !hs::nonzero(key)) return false;
    pair_=pair;psk_=key;return true;
  }
  bool generate(){
    clear();
    if(!crypto_.random(pair_.data(),pair_.size()) ||
       !crypto_.random(psk_.data(),psk_.size()) || !valid()) {clear();return false;}
    return true;
  }
private:
  const hs::Crypto& crypto_;hs::PairId pair_{};hs::Secret psk_{};
};
class Scratch {
public:
  explicit Scratch(const hs::Crypto& crypto):crypto_(crypto){}
  ~Scratch(){crypto_.wipe(bytes.data(),bytes.size());}
  Scratch(const Scratch&)=delete;
  Scratch& operator=(const Scratch&)=delete;
  Record bytes{};
private:const hs::Crypto& crypto_;
};

inline void encode(Record& b,Role role,uint32_t revision,const Material* material) {
  b={};auto* p=b.data();
  wire::put(p,0x50504352,4); // PPCR
  wire::put(p,1,1);wire::put(p,static_cast<uint8_t>(role),1);
  wire::put(p,material?1:0,1);wire::put(p,0,1);
  wire::put(p,revision,4);
  if(material) {
    std::memcpy(p,material->pair().data(),16);
    std::memcpy(p+16,material->psk().data(),32);
  }
  p=b.data()+60;wire::put(p,checksum(b.data(),60),4);
}
inline bool decode(const Record& b,Role role,uint32_t& revision,State& state,Material& out,
                   const hs::Crypto& crypto) {
  out.clear();revision=0;state=State::Fault;
  const auto* p=b.data();const auto magic=wire::get(p,4),version=wire::get(p,1),
    storedRole=wire::get(p,1),paired=wire::get(p,1),reserved=wire::get(p,1),
    rev=wire::get(p,4);
  const auto* tail=b.data()+60;
  if(!validRole(role) || magic!=0x50504352 || version!=1 ||
     storedRole!=static_cast<uint8_t>(role) || paired>1 || reserved || !rev ||
     (paired && rev==UINT32_MAX) ||
     wire::get(tail,4)!=checksum(b.data(),60)) return false;
  hs::PairId pair{};hs::Secret key{};
  std::memcpy(pair.data(),p,pair.size());std::memcpy(key.data(),p+pair.size(),key.size());
  const bool ok=paired?out.import(pair,key):(!hs::nonzero(pair) && !hs::nonzero(key));
  crypto.wipe(pair.data(),pair.size());crypto.wipe(key.data(),key.size());
  if(!ok) return false;
  revision=static_cast<uint32_t>(rev);state=paired?State::Paired:State::Revoked;return true;
}

// Single trusted device owner. No transport or persistent write is invoked
// from a network callback. Load once at boot before transport construction.
// A Fault is latched for this instance: no automatic retry/default/fallback.
// Every mutation requires revalidated physical authority and quiescence.
class Vault {
public:
  Vault(Role role,Store& store,const hs::Crypto& crypto,const Maintenance& maintenance)
    :role_(role),store_(store),crypto_(crypto),maintenance_(maintenance),material_(crypto){}
  Vault(const Vault&)=delete;
  Vault& operator=(const Vault&)=delete;
  State state() const {return state_;}
  uint32_t revision() const {return revision_;}
  // Borrow only during normal operation; owner must stop/destroy transport
  // before maintenance and before this Vault dies. No mutable reference.
  const Material* active() const {return state_==State::Paired?&material_:nullptr;}
  State load(){
    if(state_!=State::Unloaded) return state_;
    if(!validRole(role_) || !maintenance_.transportQuiescent() || !store_.protectedReady()) {
      fault();return state_;
    }
    Scratch record(crypto_);
    switch(store_.read(record.bytes)){
      case Read::Missing:state_=State::Empty;return state_;
      case Read::Found:
        if(decode(record.bytes,role_,revision_,state_,material_,crypto_)) return state_;
        break;
      case Read::Error:break;
    }
    fault();return state_;
  }
  // No direct replacement of a live pair. Revoke durably first. Old PSK is
  // never a fallback after successful revocation. If power or storage fails
  // BEFORE revocation is verified, report uncertainty: old record may remain.
  Result revoke(uint32_t expected){
    const auto check=permission(expected);
    if(check!=Result::Applied) return check;
    if(state_!=State::Paired) return Result::WrongState;
    return commit(nullptr);
  }
  // Pairing of a new device uses independent fresh high-entropy material
  // generated by the trusted station/owner; this API never exports it.
  // Existing pair must be revoked first. No default/bootstrap key.
  Result install(uint32_t expected,const Material& candidate){
    const auto check=permission(expected);
    if(check!=Result::Applied) return check;
    if(state_!=State::Empty && state_!=State::Revoked) return Result::WrongState;
    // Reserve the final revision for revocation; never install a pair
    // that cannot subsequently be revoked without counter wrap.
    if(revision_==UINT32_MAX-1 || !candidate.valid() || &candidate==&material_) return Result::Invalid;
    return commit(&candidate);
  }
private:
  Result permission(uint32_t expected) const {
    if(!maintenance_.locallyAuthorised() || !maintenance_.safeLifecycle() ||
       !maintenance_.transportQuiescent()) return Result::Denied;
    if(state_==State::Unloaded || state_==State::Fault) return Result::WrongState;
    if(expected!=revision_) return Result::Stale;
    if(revision_==UINT32_MAX) return Result::Invalid;
    return Result::Applied;
  }
  Result commit(const Material* material){
    Scratch proposed(crypto_),readback(crypto_);
    const auto next=revision_+1;
    encode(proposed.bytes,role_,next,material);
    // Immediately stop offering old material before attempting persistence.
    material_.clear();state_=State::Fault;
    if(!store_.protectedReady() || !store_.replace(proposed.bytes) ||
       store_.read(readback.bytes)!=Read::Found ||
       !crypto_.equal(proposed.bytes.data(),readback.bytes.data(),RecordSize)) {
      fault();return Result::StorageFault;
    }
    if(!decode(readback.bytes,role_,revision_,state_,material_,crypto_) || revision_!=next) {
      fault();return Result::StorageFault;
    }
    return Result::Applied;
  }
  void fault(){material_.clear();state_=State::Fault;revision_=0;}
  const Role role_;Store& store_;const hs::Crypto& crypto_;const Maintenance& maintenance_;
  Material material_;State state_=State::Unloaded;uint32_t revision_=0;
};

} // namespace pp::taster_link::pairing
