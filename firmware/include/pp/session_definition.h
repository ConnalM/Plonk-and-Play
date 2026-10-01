#pragma once
#include "core.h"

namespace pp {

enum class InputRole : uint8_t { StartFinish };
enum class LapFinishBehaviour : uint8_t { Immediate };

struct SessionInputRole {
  InputIdentity input;
  uint8_t lane;
  InputRole role;
};

// Fixed working session data. Production START creates this in Stage 8; the
// Stage 6 preparation fixture supplies the already-fixed accepted session.
class SessionDefinition {
public:
  explicit SessionDefinition(SessionInputRole assignment,uint32_t raceEntryId=1,uint32_t lapTarget=0,LapFinishBehaviour finish=LapFinishBehaviour::Immediate):assignment_(assignment),raceEntryId_(raceEntryId),lapTarget_(lapTarget),finish_(finish) {}
  bool resolve(InputIdentity input, SessionInputRole& result) const {
    if(input.device!=assignment_.input.device || input.capability!=assignment_.input.capability) return false;
    result=assignment_;return true;
  }
  uint32_t raceEntryId() const { return raceEntryId_; }
  uint32_t lapTarget() const { return lapTarget_; }
  LapFinishBehaviour finishBehaviour() const { return finish_; }
private:
  const SessionInputRole assignment_;
  const uint32_t raceEntryId_;
  const uint32_t lapTarget_;
  const LapFinishBehaviour finish_;
};

} // namespace pp
