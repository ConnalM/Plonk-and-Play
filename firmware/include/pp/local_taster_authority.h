#pragma once
#include "physical_taster_authority.h"
#if defined(ARDUINO)
#include <Arduino.h>
#endif

namespace pp {
// Integrated transport. Core retains all authority; the display client
// does not access its bus, race controller or storage service.
class LocalTasterAuthority final : public PhysicalTasterAuthority {
public:
  LocalTasterAuthority(RaceControlModule& control, Noticeboard& noticeboard,
                       Bus& bus, Bus::Endpoint tasterEndpoint,
                       const TasterPresentationPreferencesController* preferences = nullptr)
      : control_(control), noticeboard_(noticeboard), bus_(bus),
        tasterEndpoint_(tasterEndpoint), presentationPreferences_(preferences) {}

  TasterSnapshot snapshot() const override {
    TasterSnapshot result{};
    result.state = noticeboard_.current();
    result.readiness = control_.readiness();
    if (const ProposedRaceSetup* proposal = control_.proposedRaceSetup()) result.proposal = *proposal;
    result.proposalRevision = control_.proposalRevision();
    return result;
  }

  bool submitSetup(uint32_t correlation, SessionMode mode,
                   uint32_t lapTarget, uint16_t durationMinutes,
                   LapFinishBehaviour finish, uint32_t revision) override {
    Message request{};
    request.type = Type::SetupRequest;
    request.correlation = correlation;
    request.clientContext = ClientContext::StandaloneTaster;
    request.sessionMode = mode;
    request.setupLapTarget = lapTarget;
    request.setupDurationMinutes = durationMinutes;
    request.setupFinishPolicy = static_cast<uint8_t>(finish);
    request.proposalRevision = revision;
    const Delivery result = bus_.publish(tasterEndpoint_, request);
#if defined(ARDUINO)
    Serial.printf("[WP7 PHYS] SETUP mode=%u laps=%lu duration=%u result=%s correlation=%lu\n",
                  static_cast<unsigned>(mode), static_cast<unsigned long>(lapTarget),
                  static_cast<unsigned>(durationMinutes),
                  result == Delivery::Delivered ? "PUBLISHED" : "REJECTED",
                  static_cast<unsigned long>(correlation));
#endif
    return result == Delivery::Delivered;
  }

  bool submitStart(uint32_t correlation) override {
    Message request{};
    request.type = Type::StartRequest;
    request.correlation = correlation;
    request.sessionMode = SessionMode::None;
    request.clientContext = ClientContext::StandaloneTaster;
    const Delivery result = bus_.publish(tasterEndpoint_, request);
#if defined(ARDUINO)
    Serial.printf("[WP7 PHYS] START request=%s correlation=%lu\n",
                  result == Delivery::Delivered ? "PUBLISHED" : "REJECTED",
                  static_cast<unsigned long>(correlation));
#endif
    return result == Delivery::Delivered;
  }

  bool submitOperation(uint32_t correlation, SessionOperation operation,
                       bool confirmed) override {
    Message request{};
    request.type = Type::SessionOperationRequest;
    request.correlation = correlation;
    request.clientContext = ClientContext::StandaloneTaster;
    request.operation = operation;
    request.probe = confirmed ? 1U : 0U;
    const Delivery result = bus_.publish(tasterEndpoint_, request);
    return result == Delivery::Delivered;
  }

  bool result(uint32_t correlation, RequestResult& value,
              RequestRejection& rejection) const override {
    for (const CachedResult& cached : results_) {
      if (cached.valid && cached.correlation == correlation) {
        value = cached.value; rejection = cached.rejection; return true;
      }
    }
    return false;
  }

  NoticeboardState stateAt(Time now) const override { return noticeboard_.currentAt(now); }
  SessionLifecycle lifecycle() const override { return control_.state(); }
  const TasterPresentationPreferences* committedPreferences() const override {
    return presentationPreferences_ ? &presentationPreferences_->current() : nullptr;
  }

  void pollResults() override {
    Message event;
    while (bus_.receive(tasterEndpoint_, event)) {
      if (event.type != Type::RequestResult || event.correlation == 0) continue;
      CachedResult& cached = results_[resultCursor_++ % (sizeof(results_) / sizeof(results_[0]))];
      cached.correlation = event.correlation;
      cached.value = event.requestResult;
      cached.rejection = event.rejection;
      cached.valid = true;
#if defined(ARDUINO)
      Serial.printf("[WP7 PHYS] authority result correlation=%lu result=%s rejection=%u\n",
                    static_cast<unsigned long>(event.correlation),
                    event.requestResult == RequestResult::Accepted ? "ACCEPTED" : "REJECTED",
                    static_cast<unsigned>(event.rejection));
#endif
    }
  }

private:
  struct CachedResult {
    uint32_t correlation = 0;
    RequestResult value = RequestResult::Rejected;
    RequestRejection rejection = RequestRejection::None;
    bool valid = false;
  };

  RaceControlModule& control_;
  Noticeboard& noticeboard_;
  Bus& bus_;
  Bus::Endpoint tasterEndpoint_;
  const TasterPresentationPreferencesController* presentationPreferences_;
  CachedResult results_[8]{};
  uint8_t resultCursor_ = 0;
};
} // namespace pp
