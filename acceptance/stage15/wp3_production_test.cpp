#include "pp/diagnostic_production.h"

#include <cstdint>

using namespace pp::diagnostic;

static RecordView makeRecord(const SessionStatus& session, uint32_t value) {
  static uint8_t payload[2];
  payload[0] = static_cast<uint8_t>(value);
  payload[1] = static_cast<uint8_t>(value >> 8);
  RecordView record{};
  record.envelope.controllerEpoch = session.controllerEpoch;
  record.envelope.session = session.session;
  record.envelope.client.value = 17;
  record.envelope.kind = EventKind::BrowserRequest;
  record.envelope.correlation.request = value;
  record.payload = payload;
  record.payloadBytes = sizeof(payload);
  return record;
}

int main() {
  ProductionDiagnosticStorage storage;
  ProductionDiagnosticRecorder recorder(storage);
  SessionStartRequest start{};
  start.controllerEpoch.value = 99;
  start.client.value = 1;

  SessionStatus normal{};
  if (!recorder.beginNormal(start.controllerEpoch, start.client)) return 1;
  if (!recorder.status(normal) || normal.lifecycle != SessionLifecycle::Active ||
      normal.storage != StorageHealth::RamOnly || !normal.evidenceVolatile) return 2;

  SessionStatus development{};
  if (recorder.startDevelopment(start, development) != RecorderResult::Accepted ||
      development.lifecycle != SessionLifecycle::Active || development.session.value != 1) return 3;
  SessionStartRequest second = start;
  second.client.value = 2;
  SessionStatus existing{};
  if (recorder.startDevelopment(second, existing) != RecorderResult::AlreadyActive ||
      existing.session.value != development.session.value) return 4;

  for (uint32_t i = 1; i <= 3; ++i) {
    uint64_t sequence = 0;
    if (recorder.tryAppend(makeRecord(development, i), sequence) != RecorderResult::Accepted ||
        sequence != i) return 5;
  }
  if (storage.count() != 3) return 6;
  RecordEnvelope envelope{};
  uint8_t payload[2]{};
  size_t length = 0;
  if (!recorder.read(0, envelope, payload, sizeof(payload), length) ||
      envelope.sequence != 1 || envelope.session.value != 1 ||
      envelope.client.value != 17 || length != 2 || payload[0] != 1) return 7;

  recorder.tick(1);
  recorder.tick(ProductionDiagnosticRecorder::DevelopmentLimitUs + 1);
  if (!recorder.developmentStatus(development) ||
      development.lifecycle != SessionLifecycle::Completed ||
      storage.count() != 3) return 8;
  if (!recorder.status(normal) || normal.lifecycle != SessionLifecycle::Active) return 9;

  // Development mode is volatile across a controller restart, while the
  // already-retained material is still readable by the old session object.
  ProductionDiagnosticStorage restartedStorage;
  ProductionDiagnosticRecorder restarted(restartedStorage);
  if (!restarted.beginNormal(start.controllerEpoch, start.client)) return 10;
  SessionStatus afterRestart{};
  if (!restarted.developmentStatus(afterRestart) ||
      afterRestart.lifecycle != SessionLifecycle::Disabled || restarted.count() != 0) return 11;

  // The development bound is provisional and must remain explicitly bounded.
  ProductionDiagnosticStorage boundedStorage;
  ProductionDiagnosticRecorder bounded(boundedStorage);
  if (!bounded.beginNormal(start.controllerEpoch, start.client)) return 12;
  SessionStatus boundedSession{};
  for (size_t i = 0; i < ProductionDiagnosticStorage::Capacity; ++i) {
    uint64_t sequence = 0;
    if (bounded.tryAppend(makeRecord(boundedSession, static_cast<uint32_t>(i + 1)), sequence) != RecorderResult::Accepted) return 13;
  }
  uint64_t droppedSequence = 0;
  if (bounded.tryAppend(makeRecord(boundedSession, 1000), droppedSequence) != RecorderResult::Accepted ||
      droppedSequence != ProductionDiagnosticStorage::Capacity + 1 || bounded.count() != ProductionDiagnosticStorage::Capacity) return 14;
  RecordEnvelope rolled{};
  if (!bounded.read(0, rolled, payload, sizeof(payload), length) || rolled.sequence != 2) return 15;
  if (bounded.startDevelopment(start, development) != RecorderResult::Accepted) return 16;
  if (bounded.tryAppend(makeRecord(development, 1001), droppedSequence) != RecorderResult::Accepted) return 17;
  if (!bounded.status(boundedSession) || boundedSession.storage != StorageHealth::Degraded ||
      !bounded.developmentStatus(development) || !development.evidenceIncomplete ||
      development.droppedRecords != 1 || bounded.count() != ProductionDiagnosticStorage::Capacity) return 18;

  // A closed backend faults once and does not cause a retry loop.
  ProductionDiagnosticStorage failedStorage;
  ProductionDiagnosticRecorder failed(failedStorage);
  if (!failed.beginNormal(start.controllerEpoch, start.client)) return 19;
  failedStorage.close();
  uint64_t failedSequence = 0;
  if (failed.tryAppend(makeRecord(normal, 2000), failedSequence) != RecorderResult::Fault) return 20;
  SessionStatus fault{};
  if (!failed.status(fault) || fault.storage != StorageHealth::Fault ||
      fault.lastFailure != FailureReason::WriteFailure || !fault.evidenceIncomplete) return 21;
  if (failed.tryAppend(makeRecord(normal, 2001), failedSequence) != RecorderResult::Fault ||
      failedStorage.count() != 0) return 22;
  return 0;
}
