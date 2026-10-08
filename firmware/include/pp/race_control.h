#pragma once
#include "core.h"
#include "session_definition.h"
#include "history_store.h"
#include "record_store.h"

namespace pp {

enum class SessionLifecycle : uint8_t { Ready, Starting, Racing, Paused, Restarting, Finished, Faulted };

class RaceControlModule {
public:
  static constexpr Time StartLeadUs=1000000;
  static constexpr Time DefaultRedIntervalUs=1000000;
  static constexpr Time DefaultFinalDelayMinUs=500000;
  static constexpr Time DefaultFinalDelayMaxUs=2000000;
  RaceControlModule(Bus& bus,Bus::Endpoint endpoint,ActiveSessionDefinition& active, HistoryStore* history=nullptr, TrackRecordStore* records=nullptr):bus_(bus),endpoint_(endpoint),active_(active),history_(history),records_(records) {}
  // Stage 6/7 preparation supplies an already-fixed external definition. It
  // does not exercise Browser START acceptance or create a competing path.
  void resetForFixture(){definition_=nullptr;setup_=nullptr;state_=SessionLifecycle::Ready;go_=pauseAt_=scheduledRestart_=settledAt_=durationExpiry_=durationRemaining_=0;restartMethod_=RestartMethod::None;startCommitted_=false;integrityFaulted_=false;pauseSettled_=false;durationExpired_=false;durationPublished_=false;resumeScheduled_=false;changed();}
  void resetRaceForFixture(){definition_=nullptr;state_=SessionLifecycle::Ready;go_=pauseAt_=scheduledRestart_=settledAt_=durationExpiry_=durationRemaining_=0;restartMethod_=RestartMethod::None;startCommitted_=false;integrityFaulted_=false;pauseSettled_=false;durationExpired_=false;durationPublished_=false;resumeScheduled_=false;changed();}
  void prepare(const SessionDefinition& definition){definition_=&definition;setup_=nullptr;state_=SessionLifecycle::Ready;go_=pauseAt_=scheduledRestart_=settledAt_=durationExpiry_=durationRemaining_=0;restartMethod_=RestartMethod::None;startCommitted_=false;integrityFaulted_=false;pauseSettled_=false;durationExpired_=false;durationPublished_=false;resumeScheduled_=false;changed();}
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
    if(state_==SessionLifecycle::Starting&&now>=go_){
      state_=SessionLifecycle::Racing;
      if(resumeScheduled_){
        // Resume uses the same authoritative start boundary as the normal
        // Lap Race start sequence. The future Resume operation is published
        // when accepted, so the Race Engine knows the boundary before a
        // crossing at GO can arrive; this transition only makes GO visible.
        resumeScheduled_=false;
        if(mode()==SessionMode::Endurance){durationExpiry_=go_+durationRemaining_;durationRemaining_=0;durationExpired_=false;durationPublished_=false;}
        publishFact(Type::Resumed,go_);
      } else if(mode()==SessionMode::Endurance){
        durationExpiry_=go_+(definition_?definition_->durationUs():0);durationPublished_=false;
      }
      changed();
    }
    if(state_==SessionLifecycle::Racing&&mode()==SessionMode::Endurance&&!durationExpired_&&durationExpiry_&&now>=durationExpiry_){durationExpired_=true;durationRemaining_=0;if(!durationPublished_){Message expiry{};expiry.type=Type::EnduranceExpired;expiry.relevantTime=durationExpiry_;if(bus_.publish(endpoint_,expiry)==Delivery::Delivered)durationPublished_=true;}changed();}
    if(state_==SessionLifecycle::Restarting&&now>=scheduledRestart_){state_=SessionLifecycle::Racing;publishFact(Type::Resumed,now);changed();}
  }
  SessionLifecycle state()const{return state_;}
  SessionMode mode()const{return definition_?definition_->mode():(setup_?setup_->mode:SessionMode::LapRace);}
  Time scheduledGo()const{return go_;}
  Time pauseEffectiveAt()const{return pauseAt_;}
  Time scheduledRestartAt()const{return scheduledRestart_;}
  Time durationExpiryAt()const{return durationExpiry_;}
  Time durationRemaining(Time now)const{if(mode()!=SessionMode::Endurance)return 0;if(durationExpired_)return 0;if(state_==SessionLifecycle::Paused||(state_==SessionLifecycle::Starting&&resumeScheduled_))return durationRemaining_;if(durationExpiry_&&now<durationExpiry_)return durationExpiry_-now;return 0;}
  bool durationExpired()const{return durationExpired_;}
  RestartMethod restartMethod()const{return restartMethod_;}
  bool pauseSettled()const{return pauseSettled_;}
  Delivery lastFactDelivery()const{return lastFactDelivery_;}
  const SessionDefinition* definition()const{return definition_;}
  const ProposedRaceSetup* proposedRaceSetup()const{return setup_;}
  bool startCommitted()const{return startCommitted_;} bool integrityFaulted()const{return integrityFaulted_;} RaceIntegrityReason integrityReason()const{return integrityReason_;}
  uint8_t redLightCount()const{return definition_?definition_->redLightCount():5;} uint8_t startSignal()const{return definition_?definition_->startSignal():0;} uint8_t startTiming()const{return definition_?definition_->startTiming():2;} Time finalDelay()const{return finalDelay_;}
private:
  bool beginStart(Time now){
    if (definition_ && definition_->mode()==SessionMode::OpenPractice) {
      go_=0; finalDelay_=0; state_=SessionLifecycle::Racing; changed(); return true;
    }
    Time scheduledGo=0, delay=0;
    if(!calculateStartSchedule(now,scheduledGo,delay))return false;
    Message scheduled{};scheduled.type=Type::GoScheduled;scheduled.relevantTime=scheduledGo;
    if(bus_.publish(endpoint_,scheduled)!=Delivery::Delivered)return false;
    go_=scheduledGo; finalDelay_=delay; state_=SessionLifecycle::Starting; changed();return true;
  }
  bool calculateStartSchedule(Time now,Time& scheduledGo,Time& delay)const{
    if(!definition_)return false;
#if defined(PP_STAGE8_ACCEPTANCE) || defined(PP_STAGE9_ACCEPTANCE) || defined(PP_STAGE9_DEMO) || defined(PP_STAGE10_ACCEPTANCE) || defined(PP_STAGE10_DEMO) || defined(PP_STAGE11_ACCEPTANCE) || defined(PP_STAGE11_DEMO) || defined(PP_STAGE12_ACCEPTANCE) || defined(PP_STAGE12_DEMO)
    scheduledGo=now+StartLeadUs; delay=StartLeadUs;
#else
    const uint8_t reds=definition_->redLightCount();
    const Time interval=definition_->redIntervalUs();
    delay=0;
    if(definition_->startTiming()==1) delay=definition_->fixedFinalDelayUs();
    else if(definition_->startTiming()==2) {
      // A single deterministic selection is committed into the schedule. The
      // product build seeds this from the controller clock; acceptance tests
      // may drive the clock deterministically.
      const Time span=DefaultFinalDelayMaxUs-DefaultFinalDelayMinUs;
      delay=DefaultFinalDelayMinUs+((now^uint64_t(nextSessionId_)*1103515245u)%(span+1));
    }
    scheduledGo=now+Time(reds)*interval+delay;
#endif
    return true;
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
    if(request.operation==SessionOperation::ClearHistory||request.operation==SessionOperation::ClearLane1Records||request.operation==SessionOperation::ClearLane2Records||request.operation==SessionOperation::ClearTrackRecord||request.operation==SessionOperation::ClearAllRecords){
      if(request.probe!=1){reject(request.correlation,RequestRejection::ConfirmationRequired);return;}
      bool cleared=false;
      if(request.operation==SessionOperation::ClearHistory)cleared=history_&&history_->clear();
      else if(request.operation==SessionOperation::ClearLane1Records)cleared=records_&&records_->clearLane(1);
      else if(request.operation==SessionOperation::ClearLane2Records)cleared=records_&&records_->clearLane(2);
      else if(request.operation==SessionOperation::ClearTrackRecord)cleared=records_&&records_->clearTrack();
      else cleared=records_&&records_->clearAll();
      if(!cleared){reject(request.correlation,RequestRejection::StorageUnavailable);return;}
      changed();result(request.correlation,RequestResult::Accepted);return;
    }
    if(request.operation==SessionOperation::RaceAgain){
      if(state_!=SessionLifecycle::Finished||!definition_){reject(request.correlation,RequestRejection::LifecycleNotRaceAgain);return;}
      copySetupFromDefinition(); setup_=&proposedCopy_;active_.permitReplacement();state_=SessionLifecycle::Ready;startCommitted_=false;
      // RACE AGAIN starts a new session proposal. Clear all run-bound timing
      // and expiry state so a completed Endurance race cannot leak into the
      // next STARTING/RACING presentation.
      go_=pauseAt_=scheduledRestart_=settledAt_=durationExpiry_=durationRemaining_=0;
      finalDelay_=0;restartMethod_=RestartMethod::None;pauseSettled_=false;
      durationExpired_=false;durationPublished_=false;resumeScheduled_=false;
      changed();result(request.correlation,RequestResult::Accepted);return;
    }
    if(request.operation==SessionOperation::Resume){
      if(state_!=SessionLifecycle::Paused){reject(request.correlation,RequestRejection::LifecycleNotResumable);return;}
      if(!pauseSettled_){reject(request.correlation,RequestRejection::PauseSettlementPending);return;}
      if(mode()==SessionMode::Endurance&&durationExpired_){reject(request.correlation,RequestRejection::LifecycleNotResumable);return;}
      if(mode()!=SessionMode::OpenPractice){
        Time resumeGo=0,resumeDelay=0;
        if(!calculateStartSchedule(now,resumeGo,resumeDelay)){reject(request.correlation,RequestRejection::SessionDefinitionUnavailable);return;}
        // Keep paused duration frozen until authoritative GO. Race Engine
        // receives the future Relevant-Time operation immediately, preventing
        // pre-GO crossings while the normal configured countdown is presented.
        publishOperation(SessionOperation::Resume,RestartMethod::None,resumeGo);
        go_=resumeGo;finalDelay_=resumeDelay;resumeScheduled_=true;state_=SessionLifecycle::Starting;changed();result(request.correlation,RequestResult::Accepted);return;
      }
      publishOperation(SessionOperation::Resume,RestartMethod::None,now);
      state_=SessionLifecycle::Racing;publishFact(Type::Resumed,now);changed();result(request.correlation,RequestResult::Accepted);return;
    }
    if(request.operation==SessionOperation::EndSession){
      if(mode()!=SessionMode::OpenPractice || (state_!=SessionLifecycle::Racing&&state_!=SessionLifecycle::Paused)){reject(request.correlation,RequestRejection::LifecycleNotEndable);return;}
      if(state_==SessionLifecycle::Paused&&!pauseSettled_){reject(request.correlation,RequestRejection::PauseSettlementPending);return;}
      publishOperation(SessionOperation::EndSession,RestartMethod::None,now);
       active_.clearForFixture();definition_=nullptr;state_=SessionLifecycle::Ready;startCommitted_=false;go_=pauseAt_=scheduledRestart_=settledAt_=0;restartMethod_=RestartMethod::None;pauseSettled_=false;resumeScheduled_=false;changed();result(request.correlation,RequestResult::Accepted);return;
    }
    if(request.operation==SessionOperation::RestartRace||request.operation==SessionOperation::EndRace){
      if(request.probe!=1){reject(request.correlation,RequestRejection::ConfirmationRequired);return;}
      // Lap Race abandonment retains the settled-pause fence. Endurance is
      // also explicitly abandonable while actively racing; a paused
      // Endurance session still has to complete that same settlement fence.
      const bool enduranceRacing=mode()==SessionMode::Endurance&&state_==SessionLifecycle::Racing;
      const bool settledPause=state_==SessionLifecycle::Paused&&pauseSettled_;
      if(!enduranceRacing&&!settledPause){reject(request.correlation,RequestRejection::LifecycleNotAbandonable);return;}
      if(request.operation==SessionOperation::RestartRace){
        if(definition_){copySetupFromDefinition();setup_=&proposedCopy_;active_.permitReplacement();}
         active_.clearForFixture();definition_=nullptr;state_=SessionLifecycle::Ready;startCommitted_=false;go_=pauseAt_=scheduledRestart_=settledAt_=0;restartMethod_=RestartMethod::None;pauseSettled_=false;resumeScheduled_=false;changed();result(request.correlation,RequestResult::Accepted);return;
      }
       active_.clearForFixture();definition_=nullptr;setup_=nullptr;state_=SessionLifecycle::Ready;startCommitted_=false;go_=pauseAt_=scheduledRestart_=settledAt_=0;restartMethod_=RestartMethod::None;pauseSettled_=false;resumeScheduled_=false;changed();result(request.correlation,RequestResult::Accepted);return;
    }
    if(request.operation==SessionOperation::Pause){
      if(state_!=SessionLifecycle::Racing){reject(request.correlation,RequestRejection::LifecycleNotPausable);return;}
      pauseAt_=now;pauseSettled_=false;settledAt_=0;if(mode()==SessionMode::Endurance&&durationExpiry_){durationRemaining_=durationExpiry_>now?durationExpiry_-now:0;durationExpiry_=0;}publishOperation(SessionOperation::Pause,RestartMethod::None,pauseAt_);
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
    if(!setup_){reject(request.correlation,RequestRejection::InvalidRaceSetup);return;}
    ProposedRaceSetup requested=*setup_; requested.mode=request.sessionMode;
    if(request.sessionMode==SessionMode::Endurance){
      if(request.durationMinutes) requested.durationMinutes=request.durationMinutes;
      requested.finish=static_cast<LapFinishBehaviour>(request.finishPolicy);
    }
    if(!valid(requested)){reject(request.correlation,RequestRejection::InvalidRaceSetup);return;}
    if(!requiredCapabilityAvailable_){reject(request.correlation,RequestRejection::RequiredCapabilityUnavailable);return;}
    const uint32_t sessionId=nextSessionId_,firstEntryId=nextRaceEntryId_;
    if(!active_.commit(requested,sessionId,firstEntryId)){reject(request.correlation,RequestRejection::SessionDefinitionUnavailable);return;}
    ++nextSessionId_;nextRaceEntryId_+=requested.activeLanes;
    definition_=active_.current();
    startCommitted_=true;
    // The fixed data and one authoritative GO schedule exist before this
    // externally visible acceptance result.
    if(!beginStart(now)){startCommitted_=false;reject(request.correlation,RequestRejection::SessionDefinitionUnavailable);return;}
    result(request.correlation,RequestResult::Accepted);
  }
  void changed(){Message notice{};notice.type=Type::NoticeboardChanged;bus_.publish(endpoint_,notice);}
  void copySetupFromDefinition(){
    proposedCopy_={};
    proposedCopy_.lapTarget=definition_->lapTarget(); proposedCopy_.durationMinutes=definition_->durationMinutes(); proposedCopy_.finish=definition_->finishBehaviour(); proposedCopy_.activeLanes=definition_->entryCount();
    proposedCopy_.mode=definition_->mode();
    proposedCopy_.startsBeforeStartFinish=true; proposedCopy_.optionalFeaturesEnabled=false;
    proposedCopy_.redLightCount=definition_->redLightCount(); proposedCopy_.startSignal=definition_->startSignal(); proposedCopy_.startTiming=definition_->startTiming();
    proposedCopy_.redIntervalUs=definition_->redIntervalUs(); proposedCopy_.fixedFinalDelayUs=definition_->fixedFinalDelayUs();
    proposedCopy_.falseStartCapability=definition_->falseStartCapability(); proposedCopy_.falseStartPolicy=definition_->falseStartPolicy();
    for(uint8_t i=0;i<definition_->entryCount();++i){proposedCopy_.entries[i].startFinish=definition_->role(i);proposedCopy_.entries[i].mugId=definition_->entry(i).mugId;}
    // Keep accepted source-level aliases synchronized for legacy fixtures and
    // two-entry Browser/demo compatibility; indexed entries are authoritative.
    if(definition_->entryCount()>0){proposedCopy_.startFinish=definition_->role(0);proposedCopy_.selectedMugId=definition_->entry(0).mugId;}
    if(definition_->entryCount()>1){proposedCopy_.secondStartFinish=definition_->role(1);proposedCopy_.secondMugId=definition_->entry(1).mugId;}
  }
  Bus& bus_;Bus::Endpoint endpoint_;ActiveSessionDefinition& active_;HistoryStore* history_=nullptr;TrackRecordStore* records_=nullptr;const SessionDefinition* definition_=nullptr;const ProposedRaceSetup* setup_=nullptr;
  SessionLifecycle state_=SessionLifecycle::Ready;Time go_=0,pauseAt_=0,scheduledRestart_=0,settledAt_=0,finalDelay_=0,durationExpiry_=0,durationRemaining_=0;RestartMethod restartMethod_=RestartMethod::None;bool requiredCapabilityAvailable_=true,startCommitted_=false,integrityFaulted_=false,pauseSettled_=false,durationExpired_=false,durationPublished_=false,resumeScheduled_=false;Delivery lastFactDelivery_=Delivery::Invalid;RaceIntegrityReason integrityReason_=RaceIntegrityReason::None;
  uint32_t nextSessionId_=1,nextRaceEntryId_=1;
  ProposedRaceSetup proposedCopy_{};
};

} // namespace pp
