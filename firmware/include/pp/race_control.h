#pragma once
#include "core.h"
#include "session_definition.h"

namespace pp {

enum class SessionLifecycle : uint8_t { Ready, Starting, Racing, Paused, Restarting, Finished, Faulted };

class RaceControlModule {
public:
  static constexpr Time StartLeadUs=1000000;
  RaceControlModule(Bus& bus,Bus::Endpoint endpoint,ActiveSessionDefinition& active):bus_(bus),endpoint_(endpoint),active_(active) {}
  // Stage 6/7 preparation supplies an already-fixed external definition. It
  // does not exercise Browser START acceptance or create a competing path.
  void resetForFixture(){definition_=nullptr;setup_=nullptr;state_=SessionLifecycle::Ready;go_=pauseAt_=scheduledRestart_=settledAt_=0;restartMethod_=RestartMethod::None;startCommitted_=false;integrityFaulted_=false;pauseSettled_=false;changed();}
  void resetRaceForFixture(){definition_=nullptr;state_=SessionLifecycle::Ready;go_=pauseAt_=scheduledRestart_=settledAt_=0;restartMethod_=RestartMethod::None;startCommitted_=false;integrityFaulted_=false;pauseSettled_=false;changed();}
  void prepare(const SessionDefinition& definition){definition_=&definition;setup_=nullptr;state_=SessionLifecycle::Ready;go_=pauseAt_=scheduledRestart_=settledAt_=0;restartMethod_=RestartMethod::None;startCommitted_=false;integrityFaulted_=false;pauseSettled_=false;changed();}
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
      else if(event.type==Type::SessionOperationRequest) handleOperation(event,now);
      else if(event.type==Type::PauseSettled){pauseSettled_=true;settledAt_=event.relevantTime;changed();}
      else if(event.type==Type::CompetitionComplete){if(state_!=SessionLifecycle::Faulted){state_=SessionLifecycle::Finished;restartMethod_=RestartMethod::None;scheduledRestart_=0;changed();}}
      else if(event.type==Type::RaceIntegrityFault){integrityFaulted_=true;integrityReason_=event.integrityReason;state_=SessionLifecycle::Faulted;restartMethod_=RestartMethod::None;scheduledRestart_=0;changed();}
    }
    if(state_==SessionLifecycle::Starting&&now>=go_){state_=SessionLifecycle::Racing;changed();}
    if(state_==SessionLifecycle::Restarting&&now>=scheduledRestart_){state_=SessionLifecycle::Racing;publishFact(Type::Resumed,now);changed();}
  }
  SessionLifecycle state()const{return state_;}
  Time scheduledGo()const{return go_;}
  Time pauseEffectiveAt()const{return pauseAt_;}
  Time scheduledRestartAt()const{return scheduledRestart_;}
  RestartMethod restartMethod()const{return restartMethod_;}
  bool pauseSettled()const{return pauseSettled_;}
  Delivery lastFactDelivery()const{return lastFactDelivery_;}
  const SessionDefinition* definition()const{return definition_;}
  const ProposedRaceSetup* proposedRaceSetup()const{return setup_;}
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
  void publishOperation(SessionOperation op,RestartMethod method,Time at){
    Message m{};m.type=Type::SessionOperation;m.operation=op;m.restartMethod=method;m.relevantTime=at;
    bus_.publish(endpoint_,m);
  }
  void publishFact(Type type,Time at){Message f{};f.type=type;f.relevantTime=at;f.restartMethod=restartMethod_;lastFactDelivery_=bus_.publish(endpoint_,f);}
  void handleOperation(const Message& request,Time now){
    if(request.clientContext!=ClientContext::RaceDirectorSmug){reject(request.correlation,RequestRejection::PermissionDenied);return;}
    if(request.operation==SessionOperation::RaceAgain){
      if(state_!=SessionLifecycle::Finished||!definition_){reject(request.correlation,RequestRejection::LifecycleNotRaceAgain);return;}
      proposedCopy_={};proposedCopy_.startFinish=definition_->role(0);proposedCopy_.selectedMugId=definition_->entry(0).mugId;proposedCopy_.lapTarget=definition_->lapTarget();proposedCopy_.finish=definition_->finishBehaviour();proposedCopy_.activeLanes=definition_->entryCount();
      if(definition_->entryCount()==2){proposedCopy_.secondStartFinish=definition_->role(1);proposedCopy_.secondMugId=definition_->entry(1).mugId;}
      proposedCopy_.startsBeforeStartFinish=true;proposedCopy_.optionalFeaturesEnabled=false;setup_=&proposedCopy_;active_.permitReplacement();state_=SessionLifecycle::Ready;startCommitted_=false;go_=pauseAt_=scheduledRestart_=settledAt_=0;restartMethod_=RestartMethod::None;changed();result(request.correlation,RequestResult::Accepted);return;
    }
    if(request.operation==SessionOperation::Pause){
      if(state_!=SessionLifecycle::Racing){reject(request.correlation,RequestRejection::LifecycleNotPausable);return;}
      pauseAt_=now;pauseSettled_=false;settledAt_=0;publishOperation(SessionOperation::Pause,RestartMethod::None,pauseAt_);
      state_=SessionLifecycle::Paused;publishFact(Type::Paused,pauseAt_);changed();result(request.correlation,RequestResult::Accepted);return;
    }
    if(state_!=SessionLifecycle::Paused){reject(request.correlation,RequestRejection::LifecycleNotRestartable);return;}
    if(!pauseSettled_){reject(request.correlation,RequestRejection::PauseSettlementPending);return;}
    restartMethod_=request.operation==SessionOperation::HonourRestart?RestartMethod::Honour:RestartMethod::Grid;
    scheduledRestart_=now+3000000;publishOperation(request.operation,restartMethod_,scheduledRestart_);
    state_=SessionLifecycle::Restarting;publishFact(Type::RestartScheduled,scheduledRestart_);changed();result(request.correlation,RequestResult::Accepted);
  }
  void handleStart(const Message& request,Time now){
    if(request.clientContext!=ClientContext::RaceDirectorSmug){reject(request.correlation,RequestRejection::PermissionDenied);return;}
    // startCommitted_ reserves the race before a second closely arriving
    // request can be accepted, even if presentation has not yet refreshed.
    if(state_!=SessionLifecycle::Ready||startCommitted_){reject(request.correlation,RequestRejection::LifecycleNotStartable);return;}
    if(!setup_||!valid(*setup_)){reject(request.correlation,RequestRejection::InvalidRaceSetup);return;}
    if(!requiredCapabilityAvailable_){reject(request.correlation,RequestRejection::RequiredCapabilityUnavailable);return;}
    const uint32_t sessionId=nextSessionId_,firstEntryId=nextRaceEntryId_;
    if(!active_.commit(*setup_,sessionId,firstEntryId)){reject(request.correlation,RequestRejection::SessionDefinitionUnavailable);return;}
    ++nextSessionId_;nextRaceEntryId_+=setup_->activeLanes;
    definition_=active_.current();
    startCommitted_=true;
    // The fixed data exists before this externally visible acceptance result.
    result(request.correlation,RequestResult::Accepted);
    beginStart(now);
  }
  void changed(){Message notice{};notice.type=Type::NoticeboardChanged;bus_.publish(endpoint_,notice);}
  Bus& bus_;Bus::Endpoint endpoint_;ActiveSessionDefinition& active_;const SessionDefinition* definition_=nullptr;const ProposedRaceSetup* setup_=nullptr;
  SessionLifecycle state_=SessionLifecycle::Ready;Time go_=0,pauseAt_=0,scheduledRestart_=0,settledAt_=0;RestartMethod restartMethod_=RestartMethod::None;bool requiredCapabilityAvailable_=true,startCommitted_=false,integrityFaulted_=false,pauseSettled_=false;Delivery lastFactDelivery_=Delivery::Invalid;RaceIntegrityReason integrityReason_=RaceIntegrityReason::None;
  uint32_t nextSessionId_=1,nextRaceEntryId_=1;
  ProposedRaceSetup proposedCopy_{};
};

} // namespace pp
