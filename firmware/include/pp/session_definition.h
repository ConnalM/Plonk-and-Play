#pragma once
#include "core.h"
#include <new>

namespace pp {

enum class InputRole : uint8_t { StartFinish };
enum class LapFinishBehaviour : uint8_t { Immediate };

struct SessionInputRole {
  InputIdentity input;
  uint8_t lane;
  InputRole role;
};

// Mutable working choices for the next race. This is data, not an operational
// module, and Browser START never supplies a copy of it.
struct ProposedRaceSetup {
  ProposedRaceSetup() {}
  ProposedRaceSetup(SessionInputRole assignment,uint32_t mug,uint32_t target,LapFinishBehaviour finishValue,uint8_t lanes,bool startsBefore,bool optional):startFinish(assignment),selectedMugId(mug),lapTarget(target),finish(finishValue),activeLanes(lanes),startsBeforeStartFinish(startsBefore),optionalFeaturesEnabled(optional) {}
  SessionInputRole startFinish{InputIdentity{},1,InputRole::StartFinish};
  uint32_t selectedMugId=0;
  uint32_t lapTarget=0;
  LapFinishBehaviour finish=LapFinishBehaviour::Immediate;
  uint8_t activeLanes=1;
  bool startsBeforeStartFinish=true;
  bool optionalFeaturesEnabled=false;
};

inline bool valid(const ProposedRaceSetup& setup) {
  return setup.startFinish.input.device!=0 && setup.startFinish.input.capability!=0 &&
    setup.startFinish.lane==1 && setup.startFinish.role==InputRole::StartFinish &&
    setup.selectedMugId!=0 && setup.lapTarget>0 && setup.activeLanes==1 &&
    setup.startsBeforeStartFinish && !setup.optionalFeaturesEnabled;
}

// Fixed working session data. Public access is read-only; construction is the
// Race Control acceptance boundary or earlier-stage preparation scaffolding.
class SessionDefinition {
public:
  explicit SessionDefinition(SessionInputRole assignment,uint32_t raceEntryId=1,uint32_t lapTarget=0,LapFinishBehaviour finish=LapFinishBehaviour::Immediate):assignment_(assignment),raceEntryId_(raceEntryId),lapTarget_(lapTarget),finish_(finish) {}
  SessionDefinition(const ProposedRaceSetup& setup,uint32_t sessionId,uint32_t raceEntryId):assignment_(setup.startFinish),sessionId_(sessionId),raceEntryId_(raceEntryId),mugId_(setup.selectedMugId),lapTarget_(setup.lapTarget),finish_(setup.finish) {}
  bool resolve(InputIdentity input, SessionInputRole& result) const {
    if(input.device!=assignment_.input.device || input.capability!=assignment_.input.capability) return false;
    result=assignment_;return true;
  }
  uint32_t sessionId() const { return sessionId_; }
  uint32_t raceEntryId() const { return raceEntryId_; }
  uint32_t mugId() const { return mugId_; }
  uint32_t lapTarget() const { return lapTarget_; }
  LapFinishBehaviour finishBehaviour() const { return finish_; }
private:
  const SessionInputRole assignment_;
  const uint32_t sessionId_=0;
  const uint32_t raceEntryId_;
  const uint32_t mugId_=0;
  const uint32_t lapTarget_;
  const LapFinishBehaviour finish_;
};

// Shared working-RAM storage, not an operational module or Bus participant.
// Race Control commits it once for an accepted session; Race Engine reads it.
class ActiveSessionDefinition {
public:
  bool commit(const ProposedRaceSetup& setup,uint32_t sessionId,uint32_t raceEntryId) {
    if(value_ || !valid(setup)) return false;
    value_=new(storage_) SessionDefinition(setup,sessionId,raceEntryId);
    ++revision_;return true;
  }
  const SessionDefinition* current() const { return value_; }
  uint32_t revision() const { return revision_; }
private:
  alignas(SessionDefinition) uint8_t storage_[sizeof(SessionDefinition)]{};
  const SessionDefinition* value_=nullptr;
  uint32_t revision_=0;
};

} // namespace pp