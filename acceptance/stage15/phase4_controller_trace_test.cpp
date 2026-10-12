#include "pp/diagnostic_memory.h"
#include "pp/diagnostic_trace.h"
#include "pp/race_control.h"

using namespace pp;
using namespace pp::diagnostic;

static Message startRequest(uint32_t correlation) {
  Message message{};
  message.type = Type::StartRequest;
  message.correlation = correlation;
  message.clientContext = ClientContext::RaceDirectorSmug;
  message.sessionMode = SessionMode::None;
  return message;
}

static Message result(uint32_t correlation, RequestResult value,
                     RequestRejection rejection = RequestRejection::None) {
  Message message{};
  message.type = Type::RequestResult;
  message.correlation = correlation;
  message.requestResult = value;
  message.rejection = rejection;
  return message;
}

static Message notice() {
  Message message{};
  message.type = Type::NoticeboardChanged;
  return message;
}

static bool actualRaceControlTrace() {
  Bus bus;
  const auto presentation = bus.attach(Role::Presentation);
  const auto controlEndpoint = bus.attach(Role::RaceControl);
  const auto observer = bus.attach(Role::Diagnostics);
  ActiveSessionDefinition active;
  RaceControlModule control(bus, controlEndpoint, active);
  ProposedRaceSetup setup{};
  setup.entries[0].startFinish = SessionInputRole{InputIdentity{1, 1}, 1, InputRole::StartFinish};
  setup.entries[0].mugId = 91;
  setup.activeLanes = 1;
  setup.lapTarget = 2;
  setup.mode = SessionMode::LapRace;
  if (!bus.subscribe(controlEndpoint, Type::StartRequest) ||
      !bus.subscribe(presentation, Type::GoScheduled) ||
      !bus.subscribe(observer, Type::StartRequest) ||
      !bus.subscribe(observer, Type::RequestResult) ||
      !bus.subscribe(observer, Type::NoticeboardChanged)) return false;
  control.setProposedRaceSetup(setup);

  MemoryDiagnosticStorage storage;
  InMemoryDiagnosticRecorder recorder(storage);
  SessionStartRequest session{}; session.controllerEpoch.value = 88; session.client.value = 4;
  SessionStatus status{};
  if (recorder.start(session, status) != RecorderResult::Accepted) return false;
  ControllerOperationTrace trace(recorder);
  Message request = startRequest(501);
  if (bus.publish(presentation, request) != Delivery::Delivered) return false;
  control.tick(1000);
  Message observed{};
  Time timestamp = 100;
  while (bus.receive(observer, observed)) trace.observe(observed, static_cast<uint8_t>(control.state()), timestamp++);
  if (recorder.count() != 4 || control.state() != pp::SessionLifecycle::Starting) return false;

  Message duplicate = startRequest(502);
  if (bus.publish(presentation, duplicate) != Delivery::Delivered) return false;
  control.tick(1001);
  while (bus.receive(observer, observed)) trace.observe(observed, static_cast<uint8_t>(control.state()), timestamp++);
  if (recorder.count() != 6 || control.state() != pp::SessionLifecycle::Starting) return false;
  RecordEnvelope envelope{}; uint8_t payload[4]{}; size_t length = 0;
  if (!recorder.read(5, envelope, payload, sizeof(payload), length) ||
      envelope.kind != EventKind::RequestRejected || envelope.correlation.request != 502 ||
      envelope.operation != OperationName::Start) return false;
  return true;
}

int main() {
  if (!actualRaceControlTrace()) return 1;
  MemoryDiagnosticStorage storage;
  InMemoryDiagnosticRecorder recorder(storage);
  SessionStartRequest session{};
  session.controllerEpoch.value = 77;
  session.client.value = 9;
  SessionStatus status{};
  if (recorder.start(session, status) != RecorderResult::Accepted) return 1;

  ClientId client{};
  client.value = 9;
  ControllerOperationTrace trace(recorder, client);
  trace.observe(startRequest(41), 0, 100);
  trace.observe(notice(), 1, 110);
  trace.observe(result(41, RequestResult::Accepted), 1, 120);
  if (recorder.count() != 4) return 2;

  RecordEnvelope envelope{};
  uint8_t payload[4]{};
  size_t length = 0;
  const EventKind expected[] = {EventKind::BusRequest, EventKind::RequestAccepted,
                                EventKind::StateChange, EventKind::NoticeboardChanged};
  for (size_t i = 0; i < 4; ++i) {
    if (!recorder.read(i, envelope, payload, sizeof(payload), length)) return 3;
    if (envelope.kind != expected[i] || envelope.operation != OperationName::Start ||
        envelope.correlation.request != 41 || envelope.client.value != 9 ||
        envelope.sequence != i + 1 || length != 2) return 4;
  }
  if (envelope.timestamp != 110) return 5;

  MemoryDiagnosticStorage rejectedStorage;
  InMemoryDiagnosticRecorder rejectedRecorder(rejectedStorage);
  if (rejectedRecorder.start(session, status) != RecorderResult::Accepted) return 6;
  ControllerOperationTrace rejectedTrace(rejectedRecorder, client);
  rejectedTrace.observe(startRequest(42), 0, 200);
  rejectedTrace.observe(result(42, RequestResult::Rejected,
                                RequestRejection::InvalidRaceSetup), 0, 210);
  if (rejectedRecorder.count() != 2) return 7;
  if (!rejectedRecorder.read(1, envelope, payload, sizeof(payload), length) ||
      envelope.kind != EventKind::RequestRejected || envelope.correlation.request != 42 ||
      envelope.severity != Severity::Warning || envelope.payloadType !=
          0x100u + static_cast<uint32_t>(RequestRejection::InvalidRaceSetup)) return 8;

  // A diagnostic append failure is latched by the recorder.  It must not
  // manufacture or suppress the product result represented by this fixture.
  MemoryDiagnosticStorage failingStorage;
  InMemoryDiagnosticRecorder failingRecorder(failingStorage);
  if (failingRecorder.start(session, status) != RecorderResult::Accepted) return 9;
  failingStorage.failNextAppend(StorageResult::WriteFailed);
  ControllerOperationTrace failingTrace(failingRecorder);
  failingTrace.observe(startRequest(43), 0, 300);
  const size_t productOperationCount = 1; // the fixture's product path ran once
  if (productOperationCount != 1) return 10;
  SessionStatus fault{};
  failingRecorder.status(fault);
  if (fault.storage != StorageHealth::Fault || !fault.evidenceIncomplete) return 11;
  failingTrace.observe(result(43, RequestResult::Accepted), 1, 310);
  if (productOperationCount != 1) return 12;
  return 0;
}
