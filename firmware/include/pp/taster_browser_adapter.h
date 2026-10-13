#pragma once

#include "browser_interface.h"
#include "taster_surface.h"

namespace pp {

// The integrated Taster uses the same logical Browser Interface boundary as
// every other presentation client.  It has its own stable client identity,
// but it does not become a second Race Control or Race Engine.
class BrowserTasterAuthority final : public TasterAuthority {
public:
  BrowserTasterAuthority(BrowserInterface& browser, uint64_t clientId)
      : browser_(browser), clientId_(clientId) {}

  TasterSnapshot snapshot() const override {
    TasterSnapshot result{};
    result.state = browser_.current();
    result.readiness = browser_.readiness();
    result.proposalRevision = browser_.proposalRevision();
    if (const ProposedRaceSetup* proposal = browser_.proposedRaceSetup()) result.proposal = *proposal;
    return result;
  }

  bool submitSetup(uint32_t correlation, SessionMode mode, uint32_t lapTarget,
                   uint16_t durationMinutes, LapFinishBehaviour finish,
                   uint32_t revision) override {
    return browser_.submitSetup(correlation, ClientContext::RaceDirectorSmug,
                                 clientId_, mode, lapTarget, durationMinutes,
                                 static_cast<uint8_t>(finish), revision);
  }

  bool submitStart(uint32_t correlation) override {
    return browser_.submitStart(correlation, ClientContext::RaceDirectorSmug,
                                clientId_);
  }

  bool submitOperation(uint32_t correlation, SessionOperation operation,
                       bool confirmed) override {
    return browser_.submitOperation(correlation, operation,
                                    ClientContext::RaceDirectorSmug,
                                    clientId_, confirmed);
  }

  bool result(uint32_t correlation, RequestResult& value,
              RequestRejection& rejection) const override {
    BrowserInterface::ResultView view{};
    if (!browser_.requestResult(correlation, clientId_, view) || !view.ready) return false;
    value = view.result;
    rejection = view.rejection;
    return true;
  }

private:
  BrowserInterface& browser_;
  uint64_t clientId_;
};

} // namespace pp
