#pragma once
#include "core.h"
#include <new>
namespace pp {
enum class InputRole : uint8_t { StartFinish };
enum class LapFinishBehaviour : uint8_t { Immediate, CompleteCurrentLap, CompleteFullRaceDistance };
struct SessionInputRole { InputIdentity input; uint8_t lane; InputRole role; };
struct RaceEntryDefinition { uint32_t raceEntryId,mugId; uint8_t lane; constexpr RaceEntryDefinition(uint32_t id=0,uint32_t mug=0,uint8_t laneValue=0):raceEntryId(id),mugId(mug),lane(laneValue){} };
struct ProposedRaceSetup {
  SessionInputRole startFinish{InputIdentity{},1,InputRole::StartFinish};
  SessionInputRole secondStartFinish{InputIdentity{},2,InputRole::StartFinish};
  uint32_t selectedMugId=0,secondMugId=0,lapTarget=0;
  LapFinishBehaviour finish=LapFinishBehaviour::Immediate;
  uint8_t activeLanes=1; bool startsBeforeStartFinish=true,optionalFeaturesEnabled=false;
  // 0=Lights Out / Immediate / false-start OFF; 1=Green / Fixed / WARNING;
  // 2=Random / +1 LAP.  These are fixed into a Session Definition at START.
  uint8_t redLightCount=5, startSignal=0, startTiming=2, falseStartPolicy=0;
  Time redIntervalUs=1000000, fixedFinalDelayUs=1000000;
  bool falseStartCapability=false;
  ProposedRaceSetup()=default;
  ProposedRaceSetup(SessionInputRole assignment,uint32_t mug,uint32_t target,LapFinishBehaviour f,uint8_t lanes,bool startsBefore,bool optional):startFinish(assignment),selectedMugId(mug),lapTarget(target),finish(f),activeLanes(lanes),startsBeforeStartFinish(startsBefore),optionalFeaturesEnabled(optional) {}
};
inline bool valid(const ProposedRaceSetup& s) {
  const bool first=s.startFinish.input.device&&s.startFinish.input.capability&&s.startFinish.lane==1&&s.startFinish.role==InputRole::StartFinish&&s.selectedMugId;
  const bool second=s.activeLanes!=2 || (s.secondStartFinish.input.device&&s.secondStartFinish.input.capability&&s.secondStartFinish.lane==2&&s.secondStartFinish.role==InputRole::StartFinish&&s.secondMugId&&!(s.secondStartFinish.input==s.startFinish.input));
  return first&&second&&s.lapTarget>0&&(s.activeLanes==1||s.activeLanes==2)&&
      (s.redLightCount==3||s.redLightCount==5)&&s.redIntervalUs>0&&
      s.startsBeforeStartFinish&&!s.optionalFeaturesEnabled;
}
class SessionDefinition {
public:
  explicit SessionDefinition(SessionInputRole a,uint32_t id=1,uint32_t target=0,LapFinishBehaviour f=LapFinishBehaviour::Immediate):roles_{a,{}},entries_{RaceEntryDefinition{id,0,a.lane},RaceEntryDefinition{}},count_(1),lapTarget_(target),finish_(f) {}
  SessionDefinition(const ProposedRaceSetup& s,uint32_t session,uint32_t firstId):sessionId_(session),count_(s.activeLanes),lapTarget_(s.lapTarget),finish_(s.finish) {
    redLightCount_=s.redLightCount; startSignal_=s.startSignal; startTiming_=s.startTiming; redIntervalUs_=s.redIntervalUs; fixedFinalDelayUs_=s.fixedFinalDelayUs; falseStartCapability_=s.falseStartCapability; falseStartPolicy_=s.falseStartPolicy;
    roles_[0]=s.startFinish;entries_[0]=RaceEntryDefinition{firstId,s.selectedMugId,1};
    if(count_==2){roles_[1]=s.secondStartFinish;entries_[1]=RaceEntryDefinition{firstId+1,s.secondMugId,2};}
  }
  bool resolve(InputIdentity input,SessionInputRole& role,RaceEntryDefinition& entry) const { for(uint8_t i=0;i<count_;++i)if(roles_[i].input==input){role=roles_[i];entry=entries_[i];return true;}return false; }
  bool resolve(InputIdentity input,SessionInputRole& role) const { RaceEntryDefinition ignored;return resolve(input,role,ignored); }
  uint8_t entryCount()const{return count_;} const RaceEntryDefinition& entry(uint8_t i)const{return entries_[i];} const SessionInputRole& role(uint8_t i)const{return roles_[i];}
  uint32_t sessionId()const{return sessionId_;} uint32_t raceEntryId()const{return entries_[0].raceEntryId;} uint32_t mugId()const{return entries_[0].mugId;} uint32_t lapTarget()const{return lapTarget_;} LapFinishBehaviour finishBehaviour()const{return finish_;}
  uint8_t redLightCount()const{return redLightCount_;} uint8_t startSignal()const{return startSignal_;} uint8_t startTiming()const{return startTiming_;} Time redIntervalUs()const{return redIntervalUs_;} Time fixedFinalDelayUs()const{return fixedFinalDelayUs_;} bool falseStartCapability()const{return falseStartCapability_;} uint8_t falseStartPolicy()const{return falseStartPolicy_;}
private:
  SessionInputRole roles_[2]{}; RaceEntryDefinition entries_[2]{}; uint32_t sessionId_=0; uint8_t count_=1; uint32_t lapTarget_=0; LapFinishBehaviour finish_=LapFinishBehaviour::Immediate; uint8_t redLightCount_=5,startSignal_=0,startTiming_=2,falseStartPolicy_=0; Time redIntervalUs_=1000000,fixedFinalDelayUs_=1000000; bool falseStartCapability_=false;
};
class ActiveSessionDefinition { public: void clearForFixture(){value_=nullptr;replacementPermitted_=false;++revision_;} void permitReplacement(){replacementPermitted_=value_!=nullptr;} bool commit(const ProposedRaceSetup& s,uint32_t session,uint32_t entry){if((value_&&!replacementPermitted_)||!valid(s))return false;value_=new(storage_) SessionDefinition(s,session,entry);replacementPermitted_=false;++revision_;return true;} const SessionDefinition* current()const{return value_;}uint32_t revision()const{return revision_;}private:alignas(SessionDefinition)uint8_t storage_[sizeof(SessionDefinition)]{};const SessionDefinition* value_=nullptr;uint32_t revision_=0;bool replacementPermitted_=false;};
} // namespace pp
