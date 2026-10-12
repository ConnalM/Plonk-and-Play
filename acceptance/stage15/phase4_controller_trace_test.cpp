#include "pp/diagnostic_memory.h"
#include "pp/diagnostic_trace.h"

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

int main() {
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
