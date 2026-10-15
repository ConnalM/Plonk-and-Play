#include "pp/race_control.h"

using namespace pp;

static Message startRequest(uint32_t correlation) {
  Message m{};
  m.type = Type::StartRequest;
  m.correlation = correlation;
  m.clientContext = ClientContext::RaceDirectorSmug;
  m.sessionMode = SessionMode::None;
  return m;
}

static Message operation(uint32_t correlation, SessionOperation value) {
  Message m{};
  m.type = Type::SessionOperationRequest;
  m.correlation = correlation;
  m.operation = value;
  m.clientContext = ClientContext::RaceDirectorSmug;
  return m;
}

int main() {
  Bus bus;
  const auto presentation = bus.attach(Role::Presentation);
  const auto controlEndpoint = bus.attach(Role::RaceControl);
  ActiveSessionDefinition active;
  RaceControlModule control(bus, controlEndpoint, active);
  ProposedRaceSetup setup{};
  setup.mode = SessionMode::OpenPractice;
  setup.activeLanes = 2;
  setup.entries[0].startFinish = SessionInputRole{InputIdentity{1, 1}, 1, InputRole::StartFinish};
  setup.entries[0].mugId = 91;
  setup.entries[1].startFinish = SessionInputRole{InputIdentity{1, 2}, 2, InputRole::StartFinish};
  setup.entries[1].mugId = 92;
  if (!bus.subscribe(controlEndpoint, Type::StartRequest) ||
      !bus.subscribe(controlEndpoint, Type::SessionOperationRequest) ||
      !bus.subscribe(presentation, Type::RequestResult)) return 1;
  control.setProposedRaceSetup(setup);

  if (bus.publish(presentation, startRequest(1)) != Delivery::Delivered) return 2;
  control.tick(1000);
  if (control.state() != SessionLifecycle::Racing || !control.definition()) return 3;

  if (bus.publish(presentation, operation(2, SessionOperation::EndSession)) != Delivery::Delivered) return 4;
  control.tick(2000);
  if (control.state() != SessionLifecycle::Ready || control.definition() ||
      !control.proposedRaceSetup() || control.proposedRaceSetup()->mode != SessionMode::OpenPractice) return 5;

  if (bus.publish(presentation, operation(3, SessionOperation::PracticeAgain)) != Delivery::Delivered) return 6;
  control.tick(3000);
  if (control.state() != SessionLifecycle::Ready || control.definition() ||
      !control.proposedRaceSetup() || control.proposedRaceSetup()->mode != SessionMode::OpenPractice ||
      control.proposedRaceSetup()->activeLanes != 2) return 7;

  // PRACTICE AGAIN prepares only. A subsequent START is still required.
  if (bus.publish(presentation, startRequest(4)) != Delivery::Delivered) return 8;
  control.tick(4000);
  if (control.state() != SessionLifecycle::Racing || !control.definition() ||
      control.definition()->sessionId() == 0) return 9;

  if (bus.publish(presentation, operation(5, SessionOperation::PracticeAgain)) != Delivery::Delivered) return 10;
  control.tick(5000);
  Message result{};
  while (bus.receive(presentation, result)) {
    if (result.type == Type::RequestResult && result.correlation == 5 &&
        result.requestResult != RequestResult::Rejected) return 11;
  }
  return 0;
}
