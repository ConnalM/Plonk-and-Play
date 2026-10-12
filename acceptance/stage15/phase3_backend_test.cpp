#include "development_backends.h"
#include "pp/diagnostic_memory.h"

#include <cstdio>
#include <cstring>

using namespace pp::diagnostic;

static RecordView makeRecord(const SessionStatus& status, ClientId client, uint32_t request, uint8_t value) {
  static uint8_t payload[2];
  payload[0] = value;
  payload[1] = static_cast<uint8_t>(value ^ 0xA5u);
  RecordView view{};
  view.envelope.controllerEpoch = status.controllerEpoch;
  view.envelope.session = status.session;
  view.envelope.client = client;
  view.envelope.kind = EventKind::BrowserRequest;
  view.envelope.correlation.request = request;
  view.envelope.timestamp = request * 10;
  view.payload = payload;
  view.payloadBytes = sizeof(payload);
  return view;
}

int main() {
  const char* path = "stage15-phase3-development.bin";
  std::remove(path);

  // Interchangeable normal path: recorder + development file backend.
  DevelopmentFileStorage file(path);
  InMemoryDiagnosticRecorder recorder(file);
  SessionStartRequest start{}; start.controllerEpoch.value = 101; start.client.value = 1;
  SessionStatus status{};
  if (recorder.start(start, status) != RecorderResult::Accepted) return 1;
  for (uint32_t i = 0; i < 3; ++i) {
    ClientId client{}; client.value = i % 2 ? 2 : 1;
    uint64_t sequence = 0;
    if (recorder.tryAppend(makeRecord(status, client, i + 1, static_cast<uint8_t>(i)), sequence) != RecorderResult::Accepted || sequence != i + 1) return 2;
  }
  if (recorder.count() != 3) return 3;
  SessionStatus completed{};
  if (recorder.stop(completed) != RecorderResult::Accepted || completed.lifecycle != SessionLifecycle::Completed) return 4;
  if (!file.finalized() || file.status().backend != BackendKind::DevelopmentFile || !file.status().durable) return 5;

  DevelopmentFileStorage recovered(path);
  if (recovered.recover() != StorageResult::Accepted || recovered.count() != 3 || !recovered.finalized()) return 6;
  RecordEnvelope envelope{}; uint8_t payload[8]{}; size_t length = 0;
  if (!recovered.read(1, envelope, payload, sizeof(payload), length) || envelope.sequence != 2 || envelope.client.value != 2 || length != 2 || payload[0] != 1) return 7;

  // An unfinalized file is recognized as incomplete while valid frames remain
  // readable; it is not silently promoted to a completed session.
  const char* interruptedPath = "stage15-phase3-interrupted.bin";
  std::remove(interruptedPath);
  DevelopmentFileStorage interrupted(interruptedPath);
  if (interrupted.begin(start.controllerEpoch) != StorageResult::Accepted) return 8;
  SessionStatus interruptedStatus{}; interruptedStatus.controllerEpoch = start.controllerEpoch; interruptedStatus.session.value = 1;
  if (interrupted.tryAppend(makeRecord(interruptedStatus, start.client, 9, 9), 1) != StorageResult::Accepted) return 9;
  DevelopmentFileStorage interruptedRecovery(interruptedPath);
  if (interruptedRecovery.recover() != StorageResult::Incomplete || interruptedRecovery.finalized() || interruptedRecovery.count() != 1 || interruptedRecovery.status().lastFailure != FailureReason::UnfinalizedSession) return 10;

  // Header-version and checksum corruption are rejected deterministically.
  const char* versionPath = "stage15-phase3-version.bin";
  std::remove(versionPath);
  DevelopmentFileStorage versionFile(versionPath);
  if (versionFile.begin(start.controllerEpoch) != StorageResult::Accepted || !versionFile.rewriteVersionForTest(99)) return 11;
  DevelopmentFileStorage unsupported(versionPath);
  if (unsupported.recover() != StorageResult::UnsupportedVersion || unsupported.status().lastFailure != FailureReason::UnsupportedVersion) return 12;

  const char* corruptPath = "stage15-phase3-corrupt.bin";
  std::remove(corruptPath);
  DevelopmentFileStorage corruptFile(corruptPath);
  if (corruptFile.begin(start.controllerEpoch) != StorageResult::Accepted || !corruptFile.corruptByteForTest(0)) return 13;
  DevelopmentFileStorage corrupt(corruptPath);
  if (corrupt.recover() != StorageResult::Corrupt || corrupt.status().lastFailure != FailureReason::CorruptRecord) return 14;

  // Deterministic failure wrapper covers each failure boundary without timing
  // or randomness.
  MemoryDiagnosticStorage memory;
  FailureInjectionStorage injected(memory);
  injected.failNextBegin(StorageResult::Unavailable);
  if (injected.begin(start.controllerEpoch) != StorageResult::Unavailable) return 15;
  if (injected.begin(start.controllerEpoch) != StorageResult::Accepted) return 16;
  injected.failNextAppend(StorageResult::WriteFailed);
  if (injected.tryAppend(makeRecord(status, start.client, 20, 1), 1) != StorageResult::WriteFailed) return 17;
  injected.failNextRead();
  if (injected.read(0, envelope, payload, sizeof(payload), length)) return 18;
  injected.failNextClose(StorageResult::CommitFailed);
  if (injected.close() != StorageResult::CommitFailed) return 19;
  injected.failNextRecover(StorageResult::Corrupt);
  if (injected.recover() != StorageResult::Corrupt) return 20;

  std::remove(path); std::remove(interruptedPath); std::remove(versionPath); std::remove(corruptPath);
  return 0;
}

