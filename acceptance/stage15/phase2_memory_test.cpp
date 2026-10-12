#include "pp/diagnostic_memory.h"

#include <cstring>

using namespace pp::diagnostic;

static RecordView record(const SessionStatus& status, ClientId client, uint32_t request,
                         uint8_t payloadValue) {
  static uint8_t payload[1];
  payload[0] = payloadValue;
  RecordView view{};
  view.envelope.controllerEpoch = status.controllerEpoch;
  view.envelope.session = status.session;
  view.envelope.client = client;
  view.envelope.kind = EventKind::UserAction;
  view.envelope.correlation.request = request;
  view.envelope.timestamp = request;
  view.payload = payload;
  view.payloadBytes = sizeof(payload);
  return view;
}

int main() {
  MemoryDiagnosticStorage storage;
  InMemoryDiagnosticRecorder recorder(storage);
  SessionStartRequest start{};
  start.controllerEpoch.value = 42;
  start.client.value = 7;
  SessionStatus status{};
  if (recorder.start(start, status) != RecorderResult::Accepted) return 1;
  if (status.lifecycle != SessionLifecycle::Active || status.storage != StorageHealth::RamOnly || !status.evidenceVolatile) return 2;

  SessionStartRequest second = start;
  second.client.value = 8;
  SessionStatus existing{};
  if (recorder.start(second, existing) != RecorderResult::AlreadyActive) return 3;
  if (existing.session.value != status.session.value || existing.controllerEpoch.value != 42) return 4;

  for (uint32_t i = 0; i < 2; ++i) {
    uint64_t sequence = 0;
    ClientId client{};
    client.value = i == 0 ? 7u : 8u;
    RecordView view = record(status, client, i + 1, static_cast<uint8_t>(i));
    if (recorder.tryAppend(view, sequence) != RecorderResult::Accepted || sequence != i + 1) return 5;
  }
  if (recorder.count() != 2) return 6;

  RecordEnvelope first{};
  uint8_t firstPayload[2]{};
  size_t firstLength = 0;
  if (!recorder.read(0, first, firstPayload, sizeof(firstPayload), firstLength)) return 7;
  if (first.sequence != 1 || first.client.value != 7 || first.correlation.request != 1 || firstLength != 1 || firstPayload[0] != 0) return 8;

  for (uint32_t i = 2; i < MemoryDiagnosticStorage::Phase2TestCapacity; ++i) {
    uint64_t sequence = 0;
    if (recorder.tryAppend(record(status, start.client, i + 1, static_cast<uint8_t>(i)), sequence) != RecorderResult::Accepted) return 9;
  }
  const uint64_t attemptsBeforeOverflow = storage.appendAttempts();
  uint64_t droppedSequence = 0;
  if (recorder.tryAppend(record(status, start.client, 99, 99), droppedSequence) != RecorderResult::Dropped) return 10;
  SessionStatus full{};
  recorder.status(full);
  if (full.storage != StorageHealth::Degraded || !full.evidenceIncomplete || full.droppedRecords != 1 || recorder.count() != MemoryDiagnosticStorage::Phase2TestCapacity) return 11;
  if (storage.appendAttempts() != attemptsBeforeOverflow + 1) return 12;
  if (recorder.tryAppend(record(status, start.client, 100, 100), droppedSequence) != RecorderResult::Dropped) return 13;
  if (storage.appendAttempts() != attemptsBeforeOverflow + 1) return 14;

  SessionStatus completed{};
  if (recorder.stop(completed) != RecorderResult::Accepted || completed.lifecycle != SessionLifecycle::Completed) return 15;

  MemoryDiagnosticStorage failingStorage;
  InMemoryDiagnosticRecorder failingRecorder(failingStorage);
  SessionStatus failingStatus{};
  if (failingRecorder.start(start, failingStatus) != RecorderResult::Accepted) return 16;
  failingStorage.failNextAppend(StorageResult::WriteFailed);
  uint64_t sequence = 0;
  if (failingRecorder.tryAppend(record(failingStatus, start.client, 200, 1), sequence) != RecorderResult::Fault) return 17;
  const uint64_t failedAttempts = failingStorage.appendAttempts();
  SessionStatus fault{};
  failingRecorder.status(fault);
  if (fault.storage != StorageHealth::Fault || fault.lastFailure != FailureReason::WriteFailure || !fault.evidenceIncomplete) return 18;
  if (failingRecorder.tryAppend(record(failingStatus, start.client, 201, 1), sequence) != RecorderResult::Fault) return 19;
  if (failingStorage.appendAttempts() != failedAttempts) return 20;

  SessionStatus interrupted{};
  if (failingRecorder.interrupt(interrupted) != RecorderResult::Accepted || interrupted.lifecycle != SessionLifecycle::Interrupted) return 21;
  return 0;
}

