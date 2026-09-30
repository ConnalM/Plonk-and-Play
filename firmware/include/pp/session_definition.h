#pragma once
#include "core.h"

namespace pp {

enum class InputRole : uint8_t { StartFinish };

struct SessionInputRole {
  InputIdentity input;
  uint8_t lane;
  InputRole role;
};

// Fixed working session data. Race Control will create this when START is
// implemented; Stage 4 test preparation may construct this minimal form only.
class SessionDefinition {
public:
  explicit SessionDefinition(SessionInputRole assignment,uint32_t raceEntryId=1):assignment_(assignment),raceEntryId_(raceEntryId) {}
  bool resolve(InputIdentity input, SessionInputRole& result) const {
    if(input.device!=assignment_.input.device || input.capability!=assignment_.input.capability) return false;
    result=assignment_;return true;
  }
  uint32_t raceEntryId() const { return raceEntryId_; }
private:
  const SessionInputRole assignment_;
  const uint32_t raceEntryId_;
};

} // namespace pp
