#pragma once
#include "core.h"
#include "session_definition.h"

namespace pp {

enum class SessionLifecycle : uint8_t { Ready, Starting, Racing, Finished, Faulted };

class RaceControlModule {
public:
  static constexpr Time StartLeadUs=1000000;
  RaceControlModule(Bus& bus,Bus::Endpoint endpoint,ActiveSessionDefinition& active):bus_(bus),endpoint_(endpoint),active_(active) {}
  // Stage 6/7 preparation supplies an already-fixed external definition. It
  // does not exercise Browser START acceptance or create a competing path.
  void prepare(const SessionDefinition& definition){definition_=&definition;setup_=nullptr;state_=SessionLifecycle::Ready;go_=0;startCommitted_=false;changed();}
  // Stage 8 supplies the mutable working setup and current required-capability
  // availability. Race Control alone validates and commits an active session.
  void setProposedRaceSetup(const ProposedRaceSetup& setup){setup_=&setup;definition_=active_.current();}
  void setRequiredCapabilityAvailable(bool available){requiredCapabilityAvailable_=available;}
  bool start(Time now){
    if(!definition_||state_!=SessionLifecycle::Ready||startCommitted_)return false;
    return beginStart(now);
  }
  void tick(Time now){
    Message event;
    while(bus_.receive(endpoint_,event)) {
      if(event.type==Type::StartRequest) handleStart(event,now);
      else if(state_==SessionLifecycle::Racing&&event.type==Type::CompetitionComplete){state_=SessionLifecycle::Finished;changed();} else if(event.type==Type::RaceIntegrityFault){integrityFaulted_=true;integrityReason_=event.integrityReason;state_=SessionLifecycle::Faulted;changed();}
    }
    if(state_==SessionLifecycle::Starting&&now>=go_){state_=SessionLifecycle::Racing;changed();}
  }
  SessionLifecycle state()const{return state_;}
  Time scheduledGo()const{return go_;}
  const SessionDefinition* definition()const{return definition_;}
  bool startCommitted()const{return startCommitted_;} bool integrityFaulted()const{return integrityFaulted_;} RaceIntegrityReason integrityReason()const{return integrityReason_;}
private:
  bool beginStart(Time now){
    const Time scheduledGo=now+StartLeadUs;
    Message scheduled{};scheduled.type=Type::GoScheduled;scheduled.relevantTime=scheduledGo;
    if(bus_.publish(endpoint_,scheduled)!=Delivery::Delivered)return false;
    go_=scheduledGo;state_=SessionLifecycle::Starting;changed();return true;
  }
  void result(uint32_t correlation,RequestResult value,RequestRejection reason=RequestRejection::None){
    Message response{};response.type=Type::RequestResult;response.correlation=correlation;response.requestResult=value;response.rejection=reason;
    bus_.publish(endpoint_,response);
  }
  void reject(uint32_t correlation,RequestRejection reason){result(correlation,RequestResult::Rejected,reason);}
  void handleStart(const Message& request,Time now){
    if(request.clientContext!=ClientContext::RaceDirectorSmug){reject(request.correlation,RequestRejection::PermissionDenied);return;}
    // startCommitted_ reserves the race before a second closely arriving
    // request can be accepted, even if presentation has not yet refreshed.
    if(state_!=SessionLifecycle::Ready||startCommitted_){reject(request.correlation,RequestRejection::LifecycleNotStartable);return;}
    if(!setup_||!valid(*setup_)){reject(request.correlation,RequestRejection::InvalidRaceSetup);return;}
    if(!requiredCapabilityAvailable_){reject(request.correlation,RequestRejection::RequiredCapabilityUnavailable);return;}
    if(!active_.commit(*setup_,nextSessionId_++,nextRaceEntryId_++)){reject(request.correlation,RequestRejection::SessionDefinitionUnavailable);return;}
    definition_=active_.current();
    startCommitted_=true;
    // The fixed data exists before this externally visible acceptance result.
    result(request.correlation,RequestResult::Accepted);
    beginStart(now);
  }
  void changed(){Message notice{};notice.type=Type::NoticeboardChanged;bus_.publish(endpoint_,notice);}
  Bus& bus_;Bus::Endpoint endpoint_;ActiveSessionDefinition& active_;const SessionDefinition* definition_=nullptr;const ProposedRaceSetup* setup_=nullptr;
  SessionLifecycle state_=SessionLifecycle::Ready;Time go_=0;bool requiredCapabilityAvailable_=true,startCommitted_=false,integrityFaulted_=false;RaceIntegrityReason integrityReason_=RaceIntegrityReason::None;
  uint32_t nextSessionId_=1,nextRaceEntryId_=1;
};

} // namespace pp