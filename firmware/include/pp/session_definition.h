#pragma once
#include "core.h"
#include <new>

namespace pp {
enum class InputRole : uint8_t { StartFinish };
enum class LapFinishBehaviour : uint8_t { Immediate, CompleteCurrentLap, CompleteFullRaceDistance };
struct SessionInputRole { InputIdentity input; uint8_t lane; InputRole role; };
struct RaceEntryDefinition {
  uint32_t raceEntryId,mugId; uint8_t lane;
  constexpr RaceEntryDefinition(uint32_t id=0,uint32_t mug=0,uint8_t laneValue=0):raceEntryId(id),mugId(mug),lane(laneValue){}
};
struct ProposedRaceEntry { SessionInputRole startFinish{InputIdentity{},0,InputRole::StartFinish}; uint32_t mugId=0; };

struct ProposedRaceSetup {
  ProposedRaceEntry entries[PP_MAX_ENTRIES]{};
  uint8_t activeLanes=1;
  uint32_t lapTarget=0;
  LapFinishBehaviour finish=LapFinishBehaviour::Immediate;
  bool startsBeforeStartFinish=true,optionalFeaturesEnabled=false;
  SessionMode mode=SessionMode::LapRace;
  // Legacy aliases remain source-compatible with accepted Stage 1–13
  // fixtures. Core code normalises them into entries[] before validation.
  SessionInputRole startFinish{InputIdentity{},1,InputRole::StartFinish};
  SessionInputRole secondStartFinish{InputIdentity{},2,InputRole::StartFinish};
  uint32_t selectedMugId=0,secondMugId=0;
  // 0=Lights Out / Immediate / false-start OFF; 1=Green / Fixed / WARNING;
  // 2=Random / +1 LAP. These are fixed into a Session Definition at START.
  uint8_t redLightCount=5,startSignal=0,startTiming=2,falseStartPolicy=0;
  Time redIntervalUs=1000000, fixedFinalDelayUs=1000000;
  bool falseStartCapability=false;

  ProposedRaceSetup()=default;
  ProposedRaceSetup(SessionInputRole assignment,uint32_t mug,uint32_t target,LapFinishBehaviour f,uint8_t lanes,bool startsBefore,bool optional):
    activeLanes(lanes),lapTarget(target),finish(f),startsBeforeStartFinish(startsBefore),optionalFeaturesEnabled(optional),startFinish(assignment),selectedMugId(mug) {}

  void normaliseLegacyAliases() {
    if (entries[0].startFinish.input.device==0 && startFinish.input.device) {
      entries[0].startFinish=startFinish; entries[0].mugId=selectedMugId;
    }
#if PP_MAX_ENTRIES > 1
    if (entries[1].startFinish.input.device==0 && secondStartFinish.input.device) {
      entries[1].startFinish=secondStartFinish; entries[1].mugId=secondMugId;
    }
#endif
  }
};

inline ProposedRaceSetup normalised(const ProposedRaceSetup& setup) {
  ProposedRaceSetup copy=setup; copy.normaliseLegacyAliases(); return copy;
}

inline bool valid(const ProposedRaceSetup& original) {
  const ProposedRaceSetup s=normalised(original);
  if (!s.activeLanes || s.activeLanes>PP_MAX_ENTRIES ||
      (s.mode==SessionMode::LapRace && !s.lapTarget) ||
      (s.mode!=SessionMode::LapRace && s.mode!=SessionMode::OpenPractice) ||
      (s.redLightCount!=3 && s.redLightCount!=5) || !s.redIntervalUs ||
      !s.startsBeforeStartFinish || s.optionalFeaturesEnabled) return false;
  for (uint8_t i=0;i<s.activeLanes;++i) {
    const auto& assignment=s.entries[i].startFinish;
    if (!assignment.input.device || !assignment.input.capability || assignment.lane!=uint8_t(i+1) || assignment.role!=InputRole::StartFinish || !s.entries[i].mugId) return false;
    for (uint8_t j=0;j<i;++j) {
      if (assignment.input==s.entries[j].startFinish.input || assignment.lane==s.entries[j].startFinish.lane || s.entries[i].mugId==s.entries[j].mugId) return false;
    }
  }
  return true;
}

class SessionDefinition {
public:
  explicit SessionDefinition(SessionInputRole a,uint32_t id=1,uint32_t target=0,LapFinishBehaviour f=LapFinishBehaviour::Immediate):sessionId_(0),count_(1),lapTarget_(target),finish_(f) {
    roles_[0]=a; entries_[0]=RaceEntryDefinition{id,0,a.lane};
  }
  SessionDefinition(const ProposedRaceSetup& original,uint32_t session,uint32_t firstId):sessionId_(session),lapTarget_(original.lapTarget),finish_(original.finish),mode_(original.mode) {
    const ProposedRaceSetup s=normalised(original); count_=s.activeLanes;
    redLightCount_=s.redLightCount; startSignal_=s.startSignal; startTiming_=s.startTiming; redIntervalUs_=s.redIntervalUs;
    fixedFinalDelayUs_=s.fixedFinalDelayUs; falseStartCapability_=s.falseStartCapability; falseStartPolicy_=s.falseStartPolicy;
    for(uint8_t i=0;i<count_;++i){ roles_[i]=s.entries[i].startFinish; entries_[i]=RaceEntryDefinition{firstId+uint32_t(i),s.entries[i].mugId,s.entries[i].startFinish.lane}; }
  }
  bool resolve(InputIdentity input,SessionInputRole& role,RaceEntryDefinition& entry) const { for(uint8_t i=0;i<count_;++i)if(roles_[i].input==input){role=roles_[i];entry=entries_[i];return true;}return false; }
  bool resolve(InputIdentity input,SessionInputRole& role) const { RaceEntryDefinition ignored;return resolve(input,role,ignored); }
  uint8_t entryCount()const{return count_;}
  const RaceEntryDefinition& entry(uint8_t i)const{return entries_[i];}
  const SessionInputRole& role(uint8_t i)const{return roles_[i];}
  uint32_t sessionId()const{return sessionId_;}
  uint32_t raceEntryId()const{return entries_[0].raceEntryId;}
  uint32_t mugId()const{return entries_[0].mugId;}
  uint32_t lapTarget()const{return lapTarget_;}
  SessionMode mode()const{return mode_;}
  LapFinishBehaviour finishBehaviour()const{return finish_;}
  uint8_t redLightCount()const{return redLightCount_;}
  uint8_t startSignal()const{return startSignal_;}
  uint8_t startTiming()const{return startTiming_;}
  Time redIntervalUs()const{return redIntervalUs_;}
  Time fixedFinalDelayUs()const{return fixedFinalDelayUs_;}
  bool falseStartCapability()const{return falseStartCapability_;}
  uint8_t falseStartPolicy()const{return falseStartPolicy_;}
private:
  SessionInputRole roles_[PP_MAX_ENTRIES]{}; RaceEntryDefinition entries_[PP_MAX_ENTRIES]{};
  uint32_t sessionId_=0; uint8_t count_=1; uint32_t lapTarget_=0; LapFinishBehaviour finish_=LapFinishBehaviour::Immediate; SessionMode mode_=SessionMode::LapRace;
  uint8_t redLightCount_=5,startSignal_=0,startTiming_=2,falseStartPolicy_=0;
  Time redIntervalUs_=1000000,fixedFinalDelayUs_=1000000; bool falseStartCapability_=false;
};

class ActiveSessionDefinition {
public:
  void clearForFixture(){value_=nullptr;replacementPermitted_=false;++revision_;}
  void permitReplacement(){replacementPermitted_=value_!=nullptr;}
  bool commit(const ProposedRaceSetup& s,uint32_t session,uint32_t entry){if((value_&&!replacementPermitted_)||!valid(s))return false;value_=new(storage_) SessionDefinition(s,session,entry);replacementPermitted_=false;++revision_;return true;}
  const SessionDefinition* current()const{return value_;}
  uint32_t revision()const{return revision_;}
private:
  alignas(SessionDefinition) uint8_t storage_[sizeof(SessionDefinition)]{}; const SessionDefinition* value_=nullptr; uint32_t revision_=0; bool replacementPermitted_=false;
};
} // namespace pp
