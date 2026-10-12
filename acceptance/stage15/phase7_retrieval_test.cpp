#include "development_backends.h"
#include "pp/diagnostic_memory.h"
#include "pp/diagnostic_retrieval.h"

#include <cstdio>
#include <cstring>

using namespace pp::diagnostic;

static RecordView makeRecord(const SessionStatus& status, ClientId client,
                             uint32_t request, uint8_t value) {
  static uint8_t payload[3];
  payload[0] = value;
  payload[1] = static_cast<uint8_t>(value ^ 0xA5u);
  payload[2] = static_cast<uint8_t>(value + 1u);
  RecordView view{};
  view.envelope.formatVersion = RecordEnvelope::CurrentFormatVersion;
  view.envelope.controllerEpoch = status.controllerEpoch;
  view.envelope.session = status.session;
  view.envelope.client = client;
  view.envelope.kind = EventKind::BrowserRequest;
  view.envelope.operation = OperationName::Start;
  view.envelope.correlation.request = request;
  view.envelope.timestamp = request * 10;
  view.envelope.traceId = 0x1000u + request;
  view.payload = payload;
  view.payloadBytes = sizeof(payload);
  return view;
}

static bool readChunk(const DiagnosticRecorder& recorder, size_t cursor,
                      RetrievalChunk& chunk, uint8_t* payload) {
  return DiagnosticRetrieval::next(recorder, cursor, payload,
                                    DiagnosticRetrieval::MaxPayloadBytes, chunk);
}

int main() {
  ControllerEpoch epoch{}; epoch.value = 7001;
  ClientId client1{}; client1.value = 11;
  ClientId client2{}; client2.value = 22;
  SessionStartRequest start{}; start.controllerEpoch = epoch; start.client = client1;

  MemoryDiagnosticStorage memory;
  InMemoryDiagnosticRecorder recorder(memory);
  SessionStatus status{};
  if (recorder.start(start, status) != RecorderResult::Accepted ||
      status.lifecycle != SessionLifecycle::Active || status.storage != StorageHealth::RamOnly) return 1;

  uint64_t sequence = 0;
  if (recorder.tryAppend(makeRecord(status, client1, 1, 1), sequence) != RecorderResult::Accepted || sequence != 1) return 2;
  if (recorder.tryAppend(makeRecord(status, client2, 2, 2), sequence) != RecorderResult::Accepted || sequence != 2) return 3;
  if (!recorder.status(status) || status.bytesRecorded != 6) return 26;

  // Cursor retrieval is ordered, deterministic and read-only.
  uint8_t payload[DiagnosticRetrieval::MaxPayloadBytes]{};
  RetrievalChunk chunk{};
  if (!readChunk(recorder, 0, chunk, payload) || !chunk.available || chunk.envelope.sequence != 1 ||
      chunk.envelope.client.value != client1.value || chunk.payloadBytes != 3 || payload[0] != 1 || chunk.nextCursor != 1) return 4;
  if (!readChunk(recorder, 1, chunk, payload) || !chunk.available || chunk.envelope.sequence != 2 ||
      chunk.envelope.client.value != client2.value || !chunk.end || chunk.nextCursor != 2) return 5;
  if (!readChunk(recorder, 2, chunk, payload) || chunk.available || !chunk.end || chunk.retainedRecords != 2) return 6;

  // The cursor is a moving read view: an append after a prior read is visible
  // on the next cursor, without rewriting or clearing earlier evidence.
  if (recorder.tryAppend(makeRecord(status, client1, 3, 3), sequence) != RecorderResult::Accepted || sequence != 3) return 7;
  if (!readChunk(recorder, 2, chunk, payload) || !chunk.available || chunk.envelope.sequence != 3) return 8;

  // Bounded overflow is explicit and latched; later appends do not retry the
  // backend indefinitely.
  for (uint32_t i = 4; i <= MemoryDiagnosticStorage::Phase2TestCapacity; ++i) {
    if (recorder.tryAppend(makeRecord(status, client1, i, static_cast<uint8_t>(i)), sequence) != RecorderResult::Accepted) return 9;
  }
  const uint64_t attemptsBeforeDrop = memory.appendAttempts();
  if (recorder.tryAppend(makeRecord(status, client1, 99, 99), sequence) != RecorderResult::Dropped) return 10;
  const uint64_t attemptsAfterDrop = memory.appendAttempts();
  const RecorderResult secondDrop = recorder.tryAppend(makeRecord(status, client1, 100, 100), sequence);
  if (attemptsAfterDrop != attemptsBeforeDrop + 1 || secondDrop != RecorderResult::Dropped ||
      memory.appendAttempts() != attemptsAfterDrop) return 11;
  if (!recorder.status(status) || !status.evidenceIncomplete || status.droppedRecords < 2 ||
      memory.status().health != StorageHealth::Degraded) return 12;
  const uint64_t overflowDropped = status.droppedRecords;

  // An append failure latches fault and does not create a retry storm.
  MemoryDiagnosticStorage failedMemory;
  FailureInjectionStorage injected(failedMemory);
  InMemoryDiagnosticRecorder failedRecorder(injected);
  if (failedRecorder.start(start, status) != RecorderResult::Accepted) return 13;
  injected.failNextAppend(StorageResult::WriteFailed);
  if (failedRecorder.tryAppend(makeRecord(status, client1, 101, 1), sequence) != RecorderResult::Fault ||
      injected.status().health != StorageHealth::Fault) return 14;
  const uint64_t failedAttempts = failedMemory.appendAttempts();
  if (failedRecorder.tryAppend(makeRecord(status, client1, 102, 2), sequence) != RecorderResult::Fault ||
      failedMemory.appendAttempts() != failedAttempts) return 15;
  if (!failedRecorder.status(status) || !status.evidenceIncomplete || status.lastFailure != FailureReason::WriteFailure) return 16;

  // Read failure is surfaced as a failed chunk and incomplete evidence.
  MemoryDiagnosticStorage readMemory;
  FailureInjectionStorage readInjected(readMemory);
  InMemoryDiagnosticRecorder readRecorder(readInjected);
  if (readRecorder.start(start, status) != RecorderResult::Accepted) return 17;
  if (readRecorder.tryAppend(makeRecord(status, client1, 201, 1), sequence) != RecorderResult::Accepted) return 18;
  readInjected.failNextRead();
  if (DiagnosticRetrieval::next(readRecorder, 0, payload, sizeof(payload), chunk) || !chunk.readFailed || !chunk.evidenceIncomplete) return 19;

  // Development file backend obeys the same retrieval boundary after restart.
  const char* path = "stage15-phase7-retrieval.bin";
  std::remove(path);
  DevelopmentFileStorage file(path);
  InMemoryDiagnosticRecorder fileRecorder(file);
  if (fileRecorder.start(start, status) != RecorderResult::Accepted) return 20;
  if (fileRecorder.tryAppend(makeRecord(status, client2, 301, 3), sequence) != RecorderResult::Accepted) return 21;
  if (fileRecorder.stop(status) != RecorderResult::Accepted) return 22;
  DevelopmentFileStorage recovered(path);
  if (recovered.recover() != StorageResult::Accepted) return 23;
  // Retrieval can be used directly against a recovered backend through the
  // same recorder boundary once a session status is reconstructed.
  if (recovered.count() != 1) return 24;
  RecordEnvelope envelope{}; size_t length = 0;
  if (!recovered.read(0, envelope, payload, sizeof(payload), length) || envelope.sequence != 1 || length != 3) return 25;

  std::printf("PHASE7_MEASURE envelope=%zu chunk=%zu memory_capacity=%zu file_capacity=%zu file_payload=%zu retained=%zu dropped=%llu\n",
              sizeof(RecordEnvelope), sizeof(RetrievalChunk), MemoryDiagnosticStorage::Phase2TestCapacity,
              DevelopmentFileStorage::DevelopmentFileMaxRecords, DevelopmentFileStorage::DevelopmentFileMaxPayloadBytes,
              recorder.count(), static_cast<unsigned long long>(overflowDropped));
  std::remove(path);
  return 0;
}
