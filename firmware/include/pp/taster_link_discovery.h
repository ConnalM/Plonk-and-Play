#pragma once
#include "taster_link_pairing.h"
#include <algorithm>

namespace pp::taster_link::discovery {
namespace hs=handshake;
using Bssid=std::array<uint8_t,6>;
inline constexpr char CoreSsid[]="Plonk-and-Play";
inline bool validBssid(const Bssid& b) {
  return hs::nonzero(b) && !(b[0]&1);
}
struct Candidate {
  Bssid bssid{};
  uint8_t channel=0;
  int16_t rssi=0;
};
inline bool valid(const Candidate& c){
  return validBssid(c.bssid) && c.channel>=1 && c.channel<=14;
}

// BSSID is a radio locator, NOT authenticated identity. Only matching
// remembered PPTH credentials can authorise an already remembered Core.
// No auto-switch to another locator if the remembered one is absent.
struct Hint {
  Bssid bssid{};
  hs::PairId pair{};
  bool valid() const {return validBssid(bssid) && hs::nonzero(pair);}
};
enum class Read:uint8_t {Found,Missing,Error};
using HintRecord=std::array<uint8_t,32>;
class HintStore {
public:
  virtual ~HintStore()=default;
  virtual Read read(HintRecord&)=0;
  virtual bool replace(const HintRecord&)=0;
};
inline HintRecord encode(const Hint& hint){
  HintRecord b{};
  auto* p=b.data();wire::put(p,0x50504448,4); // PPDH
  wire::put(p,1,1);wire::put(p,0,1);
  std::memcpy(p,hint.bssid.data(),6);p+=6;
  std::memcpy(p,hint.pair.data(),16);p+=16;
  wire::put(p,pairing::checksum(b.data(),28),4);
  return b;
}
inline bool decode(const HintRecord& b,Hint& out) {
  out={};const auto* p=b.data();
  if(wire::get(p,4)!=0x50504448 || wire::get(p,1)!=1 || wire::get(p,1)!=0) return false;
  Hint h;std::memcpy(h.bssid.data(),p,6);p+=6;
  std::memcpy(h.pair.data(),p,16);p+=16;
  if(wire::get(p,4)!=pairing::checksum(b.data(),28) || !h.valid()) return false;
  out=h;return true;
}

enum class State:uint8_t {
  Unloaded,Idle,Scanning,NoneFound,Choose,WaitingRemembered,Connecting,
  Connected,Retry,Incomplete,IdentityMismatch,Fault
};
enum class Credential:uint8_t { Empty,Paired,Fault };
enum class Intent:uint8_t { FirstUse,Remembered };
struct Attempt {
  uint64_t token=0;
  Candidate candidate{};
  Intent intent=Intent::FirstUse;
  hs::PairId expectedPair{};
  bool valid() const {return token && discovery::valid(candidate);}
};

// Owner-thread policy only. No Wi-Fi calls or race commands. Scan results
// are untrusted advertisements. Complete scan is an explicit platform
// assertion, not proof all radios in range were heard. No RSSI selection.
class Selector {
public:
  static constexpr size_t MaxCandidates=8;
  static constexpr uint64_t ChoiceUs=10000000;
  explicit Selector(HintStore& store):store_(store){}
  Selector(const Selector&)=delete;
  Selector& operator=(const Selector&)=delete;
  State state() const{return state_;}
  const Hint& remembered() const{return hint_;}
  const Attempt& attempt() const{return attempt_;}
  size_t count() const{return count_;}
  const Candidate* candidate(size_t i) const{return i<count_?&candidates_[i]:nullptr;}
  uint64_t scanToken() const{return scanToken_;}

  // Load after the vault. A paired vault with missing/mismatched hint is
  // an interrupted/unknown commissioning state, never fresh first-use.
  State load(Credential credential,const hs::PairId& activePair={}) {
    if(state_!=State::Unloaded) return state_;
    HintRecord bytes{};
    const auto result=store_.read(bytes);
    if(credential==Credential::Fault ||
       (credential==Credential::Paired && !hs::nonzero(activePair)) ||
       (credential==Credential::Empty && hs::nonzero(activePair))) return fault();
    if(result==Read::Error) return fault();
    if(result==Read::Found) {
      if(!decode(bytes,hint_)) return fault();
      if(credential!=Credential::Paired || hint_.pair!=activePair) return mismatch();
    } else if(credential==Credential::Paired) return mismatch();
    credential_=credential;expected_=activePair;state_=State::Idle;return state_;
  }
  // Caller must not scan while associated/authenticated or attempting
  // association. Epoch and token never wrap or allow stale selection.
  uint64_t beginScan(uint64_t now){
    if(!clock(now) || !canScan()) return 0;
    if(!next(scanToken_)) return 0;
    count_=0;incomplete_=false;attempt_={};state_=State::Scanning;return scanToken_;
  }
  // Platform iterates a bounded full scan; every same-SSID advertisement
  // counts, including duplicate SSIDs. Deduplicate only identical BSSID.
  bool observe(uint64_t token,const Candidate& c) {
    if(state_!=State::Scanning || token!=scanToken_) return false;
    if(!valid(c)) {incomplete_=true;return false;}
    for(size_t i=0;i<count_;++i) if(candidates_[i].bssid==c.bssid) {
      if(candidates_[i].channel!=c.channel) {incomplete_=true;return false;}
      candidates_[i].rssi=std::max(candidates_[i].rssi,c.rssi);return true;
    }
    if(count_==MaxCandidates) {incomplete_=true;return false;}
    candidates_[count_++]=c;return true;
  }
  State finishScan(uint64_t token,bool complete,uint64_t now){
    if(!clock(now)) return state_;
    if(state_!=State::Scanning || token!=scanToken_) return state_;
    if(!complete || incomplete_) {count_=0;state_=State::Incomplete;return state_;}
    std::sort(candidates_.begin(),candidates_.begin()+count_,
      [](const Candidate& a,const Candidate& b){return a.bssid<b.bssid;});
    if(hint_.valid()) {
      for(size_t i=0;i<count_;++i) if(candidates_[i].bssid==hint_.bssid) {
        return start(candidates_[i],Intent::Remembered);
      }
      state_=State::WaitingRemembered;return state_;
    }
    if(!count_) {state_=State::NoneFound;return state_;}
    if(count_==1) return start(candidates_[0],Intent::FirstUse);
    chosenAt_=now;state_=State::Choose;return state_;
  }
  bool choose(uint64_t token,const Bssid& b,uint64_t now) {
    if(!clock(now) || state_!=State::Choose || token!=scanToken_) return false;
    if(now-chosenAt_>=ChoiceUs) {state_=State::Retry;count_=0;return false;}
    for(size_t i=0;i<count_;++i) if(candidates_[i].bssid==b)
      return start(candidates_[i],Intent::FirstUse)==State::Connecting;
    return false;
  }
  // Only the connection owner may call this AFTER association to the
  // selected BSSID AND successful mutual PPTH authentication using the
  // committed vault material. First-use requires separately approved
  // bootstrap with persisted credentials; this method does not bootstrap.
  // Pair ID alone is not proof of authentication; the owner is responsible.
  bool authenticated(uint64_t token,const Bssid& associated,const hs::PairId& verifiedPair) {
    if(state_!=State::Connecting || token!=attempt_.token) return false;
    if(associated!=attempt_.candidate.bssid || !hs::nonzero(verifiedPair) ||
       (attempt_.intent==Intent::Remembered && verifiedPair!=expected_)) {
      mismatch();return false;
    }
    if(attempt_.intent==Intent::FirstUse) {
      const Hint h{associated,verifiedPair};const auto bytes=encode(h);
      HintRecord readback{};
      if(!store_.replace(bytes) || store_.read(readback)!=Read::Found || readback!=bytes) {
        fault();return false;
      }
      hint_=h;expected_=verifiedPair;credential_=Credential::Paired;
    }
    state_=State::Connected;return true;
  }
  // First-use failure after possible vault mutation is uncertain, so
  // require owner recovery/reload, not auto-try another Core. A remembered
  // failed link retains the hint and retries only that locator.
  bool failed(uint64_t token,bool credentialUnchanged){
    if(state_!=State::Connecting || token!=attempt_.token) return false;
    if(!credentialUnchanged) {mismatch();return true;}
    attempt_={};state_=State::Retry;return true;
  }
  void disconnected(){
    if(state_==State::Connected){attempt_={};state_=State::Retry;}
  }
  void tick(uint64_t now) {
    if(!clock(now)) return;
    if(state_==State::Choose && now-chosenAt_>=ChoiceUs){state_=State::Retry;count_=0;}
  }
private:
  bool canScan() const {
    return state_==State::Idle || state_==State::NoneFound ||
      state_==State::WaitingRemembered || state_==State::Retry || state_==State::Incomplete;
  }
  bool clock(uint64_t now) {
    if(timeSeen_ && now<lastNow_){fault();return false;}
    timeSeen_=true;lastNow_=now;return true;
  }
  bool next(uint64_t& counter){if(counter==UINT64_MAX){fault();return false;}++counter;return true;}
  State start(const Candidate& c,Intent intent){
    if(!next(attemptCounter_)) return state_;
    attempt_={attemptCounter_,c,intent,expected_};state_=State::Connecting;return state_;
  }
  State fault(){attempt_={};count_=0;state_=State::Fault;return state_;}
  State mismatch(){attempt_={};count_=0;state_=State::IdentityMismatch;return state_;}
  HintStore& store_;Hint hint_{};hs::PairId expected_{};
  Credential credential_=Credential::Empty;
  std::array<Candidate,MaxCandidates> candidates_{};size_t count_=0;
  State state_=State::Unloaded;Attempt attempt_{};
  uint64_t scanToken_=0,attemptCounter_=0,lastNow_=0,chosenAt_=0;
  bool incomplete_=false,timeSeen_=false;
};

} // namespace pp::taster_link::discovery
