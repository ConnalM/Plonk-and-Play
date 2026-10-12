#include <cstdio>
#include "pp/diagnostic_memory.h"
#include "pp/diagnostic_trace.h"

using namespace pp;
using namespace pp::diagnostic;

static bool readKind(InMemoryDiagnosticRecorder& recorder, size_t index,
                     EventKind expected, ClientId client, uint64_t traceId,
                     uint32_t revision) {
  RecordEnvelope envelope{};
  uint8_t payload[4]{};
  size_t length = 0;
  return recorder.read(index, envelope, payload, sizeof(payload), length) &&
         envelope.kind == expected && envelope.client.value == client.value &&
         envelope.traceId == traceId && envelope.correlation.event == revision;
}

int main() {
  MemoryDiagnosticStorage storage;
  InMemoryDiagnosticRecorder recorder(storage);
  SessionStartRequest start{};
  start.controllerEpoch.value = 700;
  start.client.value = 1;
  SessionStatus status{};
  if (recorder.start(start, status) != RecorderResult::Accepted) return 1;
  ControllerOperationTrace trace(recorder);
  ClientId browserA{}; browserA.value = 41;
  ClientId browserB{}; browserB.value = 42;
  const uint64_t startTrace = 0xABC1;
  trace.observeBrowserNotification(browserA, startTrace, 17, 9, 88, 1,
                                   OperationName::Start, 100);
  trace.observeBrowserState(EventKind::StateFetchStarted, browserA, startTrace,
                            17, 9, 0, 0, 0, OperationName::Start, 101);
  trace.observeBrowserState(EventKind::StateReceived, browserA, startTrace,
                            17, 9, 2, 1, 0, OperationName::Start, 102);
  trace.observeBrowserState(EventKind::StateApplied, browserA, startTrace,
                            17, 9, 2, 1, 0, OperationName::Start, 103);
  trace.observeBrowserState(EventKind::BrowserRender, browserA, startTrace,
                            17, 9, 2, 1, 1, OperationName::Start, 104);
  if (recorder.count() != 5) return 2;
  const EventKind complete[] = {EventKind::BrowserNotification,
                                EventKind::StateFetchStarted,
                                EventKind::StateReceived,
                                EventKind::StateApplied,
                                EventKind::BrowserRender};
  for (size_t i = 0; i < 5; ++i)
    if (!readKind(recorder, i, complete[i], browserA, startTrace, 9)) return 3;

  // A second Browser observes the same authoritative state and remains
  // attributable independently; this uses the same recorder/session.
  const uint64_t reconnectTrace = 0;
  trace.observeBrowserState(EventKind::Reconstruction, browserB, reconnectTrace,
                            0, 9, 3, 1, 1, OperationName::Unknown, 200);
  trace.observeBrowserState(EventKind::StateApplied, browserB, reconnectTrace,
                            0, 9, 3, 1, 0, OperationName::Unknown, 201);
  trace.observeBrowserState(EventKind::BrowserRender, browserB, reconnectTrace,
                            0, 9, 3, 1, 1, OperationName::Unknown, 202);
  if (recorder.count() != 8 || !readKind(recorder, 5, EventKind::Reconstruction,
                                          browserB, 0, 9) ||
      !readKind(recorder, 7, EventKind::BrowserRender, browserB, 0, 9)) return 4;

  // An incomplete chain remains diagnosable by the absence of later phases;
  // no synthetic application/render event is manufactured.
  MemoryDiagnosticStorage incompleteStorage;
  InMemoryDiagnosticRecorder incompleteRecorder(incompleteStorage);
  if (incompleteRecorder.start(start, status) != RecorderResult::Accepted) return 5;
  ControllerOperationTrace incomplete(incompleteRecorder);
  incomplete.observeBrowserNotification(browserA, 0xBEEF, 0, 4, 0, 1,
                                         OperationName::Unknown, 300);
  incomplete.observeBrowserState(EventKind::StateReceived, browserA, 0xBEEF,
                                  0, 4, 3, 1, 0, OperationName::Unknown, 301);
  if (incompleteRecorder.count() != 2) return 6;
  RecordEnvelope missing{}; uint8_t payload[4]{}; size_t length = 0;
  if (incompleteRecorder.read(0, missing, payload, sizeof(payload), length) &&
      missing.kind == EventKind::StateApplied) return 7;

  // Recorder/backend failure is observation loss only and cannot be a product
  // authority path.  The recorder latches the fault without retrying.
  MemoryDiagnosticStorage failingStorage;
  InMemoryDiagnosticRecorder failingRecorder(failingStorage);
  if (failingRecorder.start(start, status) != RecorderResult::Accepted) return 8;
  failingStorage.failNextAppend(StorageResult::WriteFailed);
  ControllerOperationTrace failed(failingRecorder);
  failed.observeBrowserNotification(browserA, startTrace, 17, 9, 88, 1,
                                    OperationName::Start, 400);
  if (failingRecorder.count() != 0) return 9;
  SessionStatus failedStatus{};
  if (!failingRecorder.status(failedStatus) || failedStatus.storage != StorageHealth::Fault ||
      failedStatus.droppedRecords != 1) return 10;
  failed.observeBrowserState(EventKind::BrowserRender, browserA, startTrace,
                             17, 9, 2, 1, 1, OperationName::Start, 401);
  if (failingStorage.appendAttempts() != 1) return 11;
  return 0;
}
