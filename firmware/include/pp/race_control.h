#pragma once
#include "core.h"
#include "session_definition.h"

namespace pp {

enum class SessionLifecycle : uint8_t { Ready, Starting, Racing, Finished };

class RaceControlModule {
public:
  static constexpr Time StartLeadUs=1000000;
  RaceControlModule(Bus& bus,Bus::Endpoint endpoint):bus_(bus),endpoint_(endpoint) {}
  // Stage 6 preparation supplies fixed working data. It never sets lifecycle
  // state beyond the real prepared-session READY boundary.
  void prepare(const SessionDefinition& definition){definition_=&definition;state_=SessionLifecycle::Ready;go_=0;changed();}
  bool start(Time now){
    if(!definition_||state_!=SessionLifecycle::Ready)return false;
    const Time scheduledGo=now+StartLeadUs;
    Message scheduled{};scheduled.type=Type::GoScheduled;scheduled.relevantTime=scheduledGo;
    if(bus_.publish(endpoint_,scheduled)!=Delivery::Delivered)return false;
    go_=scheduledGo;state_=SessionLifecycle::Starting;changed();return true;
  }
  void tick(Time now){
    if(state_==SessionLifecycle::Starting&&now>=go_){state_=SessionLifecycle::Racing;changed();}
    Message event;while(bus_.receive(endpoint_,event))if(state_==SessionLifecycle::Racing&&event.type==Type::CompetitionComplete){state_=SessionLifecycle::Finished;changed();}
  }
  SessionLifecycle state()const{return state_;}
  Time scheduledGo()const{return go_;}
private:
  void changed(){Message notice{};notice.type=Type::NoticeboardChanged;bus_.publish(endpoint_,notice);}
  Bus& bus_;Bus::Endpoint endpoint_;const SessionDefinition* definition_=nullptr;
  SessionLifecycle state_=SessionLifecycle::Ready;Time go_=0;
};

} // namespace pp
