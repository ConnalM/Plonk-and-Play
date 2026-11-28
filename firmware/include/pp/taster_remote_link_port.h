#pragma once

#include "taster_link_health.h"

namespace pp::taster_link {

// Narrow renderer-facing boundary for the independently-built Taster.  It
// exposes authenticated observations and command outcomes, never sporting
// authority, storage, or a transport bypass.  Keeping this separate from the
// ESP owner makes the remote presentation contract host-testable.
class RemoteTasterLinkPort {
 public:
  virtual ~RemoteTasterLinkPort() = default;
  virtual health::View view() = 0;
  virtual void observeReleased() = 0;
  virtual bool beginTouch() = 0;
  virtual void cancelTouch() = 0;
  virtual bool command(Action action, Mode mode, uint16_t value,
                       bool confirmed, uint64_t& commandId) = 0;
  virtual bool takeResult(uint64_t commandId, Status& status) = 0;
  virtual void acknowledgeUncertainty() = 0;
};

}  // namespace pp::taster_link
