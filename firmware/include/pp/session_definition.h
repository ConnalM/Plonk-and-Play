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
  explicit SessionDefinition(SessionInputRole assignment):assignment_(assignment) {}
  bool resolve(InputIdentity input, SessionInputRole& result) const {
    if(input.device!=assignment_.input.device || input.capability!=assignment_.input.capability) return false;
    result=assignment_;return true;
  }
private:
  const SessionInputRole assignment_;
};

} // namespace pp
