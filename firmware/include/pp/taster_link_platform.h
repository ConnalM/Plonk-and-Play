#pragma once
#include "taster_link_socket_runtime.h"
#include "taster_link_pairing.h"
#include <atomic>

namespace pp::taster_link::platform {
namespace sr=socket_runtime;

// Boot-lifetime event counter; callbacks only report invalidation, never
// scan, close a socket, call Race Control or mutate credentials. Saturate
// rather than wrap. Owner must receive every relevant driver/IP event.
class EventRevision {
public:
  void changed(){
    uint32_t n=value_.load(std::memory_order_relaxed);
    while(n!=UINT32_MAX &&
          !value_.compare_exchange_weak(n,n+1,std::memory_order_release,std::memory_order_relaxed)){}
  }
  uint32_t value() const{return value_.load(std::memory_order_acquire);}
private:
  std::atomic<uint32_t> value_{1};
};

// Single owner samples actual interface state. Generation changes even
// when an event disconnects/reconnects with identical IPs between polls.
// A failed/inconsistent observation invalidates the previous live epoch.
class NetworkEpoch {
public:
  sr::Network observe(sr::Network sample,uint32_t before,uint32_t after){
    sample.generation=0;
    if(!before || before!=after || after==UINT32_MAX)sample={};
    if(!sample.ready)sample={};
    if(!seen_ || before!=revision_ || after!=revision_ || !sample.same(previous_)){
      seen_=true;revision_=after;previous_=sample;
      if(generation_==UINT64_MAX){fault_=true;}
      else ++generation_;
    }
    if(after==UINT32_MAX)fault_=true;
    if(fault_ || !sample.ready)return {};
    sample.generation=generation_;return sample;
  }
  void invalidate(){seen_=false;previous_={};}
  bool faulted() const{return fault_;}
private:
  sr::Network previous_{};
  uint32_t revision_=0;uint64_t generation_=0;
  bool seen_=false,fault_=false;
};

// Owner-only local approval, not HTTP/wire fields. Explicit source of
// authority for vault use; caller must own the isolated store and every
// credential borrower. Startup permits load but not install/revoke until
// runtime commissioning and lifecycle conditions are established.
class LocalMaintenance final:public pairing::Maintenance {
public:
  void update(bool commissioned,bool protectedReady,bool safe,bool noBorrowers){
    approved_=commissioned && protectedReady;
    safe_=safe;quiet_=noBorrowers;
  }
  bool locallyAuthorised() const override{return approved_;}
  bool safeLifecycle() const override{return safe_;}
  bool transportQuiescent() const override{return quiet_;}
private:
  bool approved_=false,safe_=false,quiet_=false;
};
} // namespace pp::taster_link::platform
