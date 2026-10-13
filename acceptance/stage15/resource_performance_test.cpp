#include "development_backends.h"
#include "pp/diagnostic_memory.h"
#include "pp/diagnostic_retrieval.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <vector>

using namespace pp::diagnostic;
namespace fs = std::filesystem;

static RecordView makeRecord(const SessionStatus& status, ClientId client,
                             uint32_t request, const uint8_t* payload,
                             size_t payloadBytes) {
  RecordView view{};
  view.envelope.controllerEpoch = status.controllerEpoch;
  view.envelope.session = status.session;
  view.envelope.client = client;
  view.envelope.formatVersion = RecordEnvelope::CurrentFormatVersion;
  view.envelope.kind = EventKind::BrowserRequest;
  view.envelope.operation = OperationName::Start;
  view.envelope.correlation.request = request;
  view.envelope.timestamp = request;
  view.envelope.traceId = 0x10000000ULL + request;
  view.payload = payload;
  view.payloadBytes = payloadBytes;
  return view;
}

static SessionStatus startRecorder(InMemoryDiagnosticRecorder& recorder,
                                   uint64_t epoch = 1, uint64_t client = 1) {
  SessionStartRequest request{};
  request.controllerEpoch.value = epoch;
  request.client.value = client;
  SessionStatus status{};
  assert(recorder.start(request, status) == RecorderResult::Accepted);
  return status;
}

static uint64_t elapsedUs(const std::chrono::steady_clock::time_point& before) {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - before).count());
}

static void appendAccepted(InMemoryDiagnosticRecorder& recorder,
                           const SessionStatus& status, uint32_t request,
                           const std::vector<uint8_t>& payload) {
  ClientId client{};
  client.value = request % 2U + 1U;
  uint64_t sequence = 0;
  const RecordView view = makeRecord(status, client, request,
                                     payload.data(), payload.size());
  const RecorderResult result = recorder.tryAppend(view, sequence);
  if (result != RecorderResult::Accepted) {
    std::cerr << "appendAccepted failure result=" << static_cast<int>(result)
              << " payload=" << payload.size() << " epoch=" << status.controllerEpoch.value
              << " session=" << status.session.value << " format=" << view.envelope.formatVersion
              << "\n";
  }
  assert(result == RecorderResult::Accepted);
}

static void measureFileRecord(const std::string& path, size_t payloadBytes) {
  std::remove(path.c_str());
  DevelopmentFileStorage file(path);
  SessionStartRequest request{};
  request.controllerEpoch.value = 22;
  request.client.value = 1;
  SessionStatus status{};
  InMemoryDiagnosticRecorder recorder(file);
  assert(recorder.start(request, status) == RecorderResult::Accepted);
  std::vector<uint8_t> payload(payloadBytes, 0x5A);
  appendAccepted(recorder, status, 1, payload);
  assert(recorder.stop(status) == RecorderResult::Accepted);
  std::cout << "MEASURED file_bytes_payload_" << payloadBytes << "="
            << fs::file_size(path) << "\n";
  std::remove(path.c_str());
}

static void measureFileScaling(const std::string& path, size_t recordCount,
                               size_t payloadBytes) {
  std::remove(path.c_str());
  DevelopmentFileStorage file(path);
  SessionStartRequest request{};
  request.controllerEpoch.value = 23;
  request.client.value = 1;
  SessionStatus status{};
  InMemoryDiagnosticRecorder recorder(file);
  assert(recorder.start(request, status) == RecorderResult::Accepted);
  std::vector<uint8_t> payload(payloadBytes, 0x6B);
  for (size_t i = 0; i < recordCount; ++i) {
    appendAccepted(recorder, status, static_cast<uint32_t>(i + 1), payload);
  }
  assert(recorder.stop(status) == RecorderResult::Accepted);
  std::cout << "MEASURED file_scaling_records_" << recordCount
            << "_payload_" << payloadBytes << "=" << fs::file_size(path) << "\n";
  std::remove(path.c_str());
}

static uint64_t retrieveAll(const DiagnosticRecorder& recorder,
                            size_t& returned, size_t& payloadBytes) {
  std::vector<uint8_t> payload(DiagnosticRetrieval::MaxPayloadBytes);
  RetrievalChunk chunk{};
  size_t cursor = 0;
  returned = 0;
  payloadBytes = 0;
  const auto before = std::chrono::steady_clock::now();
  for (;;) {
    const bool ok = DiagnosticRetrieval::next(recorder, cursor, payload.data(),
                                                payload.size(), chunk);
    assert(ok);
    if (!chunk.available) break;
    ++returned;
    payloadBytes += chunk.payloadBytes;
    cursor = chunk.nextCursor;
  }
  return elapsedUs(before);
}

int main() {
  std::cout << "MEASURED sizeof_RecordEnvelope=" << sizeof(RecordEnvelope) << "\n";
  std::cout << "MEASURED sizeof_RecordView=" << sizeof(RecordView) << "\n";
  std::cout << "MEASURED sizeof_RetrievalChunk=" << sizeof(RetrievalChunk) << "\n";
  std::cout << "MEASURED sizeof_SessionStatus=" << sizeof(SessionStatus) << "\n";
  std::cout << "MEASURED sizeof_SessionStartRequest=" << sizeof(SessionStartRequest) << "\n";
  std::cout << "MEASURED sizeof_MemoryDiagnosticStorage="
            << sizeof(MemoryDiagnosticStorage) << "\n";
  std::cout << "MEASURED sizeof_InMemoryDiagnosticRecorder="
            << sizeof(InMemoryDiagnosticRecorder) << "\n";
  std::cout << "MEASURED sizeof_ControllerTracePendingCapacity=8\n";
  std::cout << "CONFIGURED memory_records="
            << MemoryDiagnosticStorage::Phase2TestCapacity
            << " memory_payload_bytes="
            << MemoryDiagnosticStorage::Phase2TestMaxPayloadBytes << "\n";
  std::cout << "CONFIGURED file_records="
            << DevelopmentFileStorage::DevelopmentFileMaxRecords
            << " file_payload_bytes="
            << DevelopmentFileStorage::DevelopmentFileMaxPayloadBytes << "\n";
  std::cout << "CONFIGURED retrieval_payload_bytes="
            << DiagnosticRetrieval::MaxPayloadBytes << "\n";

  const std::vector<uint8_t> small(3, 0x11);
  const std::vector<uint8_t> representative(64, 0x22);
  const std::vector<uint8_t> maximum(MemoryDiagnosticStorage::Phase2TestMaxPayloadBytes, 0x33);

  // Sustained recording and count-pressure overload.
  MemoryDiagnosticStorage memory;
  InMemoryDiagnosticRecorder recorder(memory);
  const SessionStatus status = startRecorder(recorder, 10, 7);
  const auto sustainedBefore = std::chrono::steady_clock::now();
  for (uint32_t i = 1; i <= 200; ++i) {
    const std::vector<uint8_t>& payload = i % 3U == 0 ? representative : small;
    const ClientId client{static_cast<uint64_t>(i % 3U + 1U)};
    uint64_t sequence = 0;
    RecordView view = makeRecord(status, client, i, payload.data(), payload.size());
    switch (i % 6U) {
      case 0: view.envelope.kind = EventKind::UserAction; break;
      case 1: view.envelope.kind = EventKind::BrowserRequest; break;
      case 2: view.envelope.kind = EventKind::RequestAccepted; break;
      case 3: view.envelope.kind = EventKind::StateChange; break;
      case 4: view.envelope.kind = EventKind::NoticeboardChanged; break;
      default: view.envelope.kind = EventKind::BrowserRender; break;
    }
    recorder.tryAppend(view, sequence);
  }
  const uint64_t sustainedUs = elapsedUs(sustainedBefore);
  SessionStatus sustained{};
  recorder.status(sustained);
  assert(recorder.count() == MemoryDiagnosticStorage::Phase2TestCapacity);
  assert(sustained.droppedRecords == 192 && sustained.evidenceIncomplete);
  assert(sustained.storage == StorageHealth::Degraded);
  assert(memory.appendAttempts() == 9);
  RecordEnvelope first{};
  uint8_t firstPayload[128]{};
  size_t firstLength = 0;
  assert(recorder.read(0, first, firstPayload, sizeof(firstPayload), firstLength));
  assert(first.sequence == 1);
  std::cout << "MEASURED sustained_attempts=200 retained=" << recorder.count()
            << " dropped=" << sustained.droppedRecords
            << " backend_append_calls=" << memory.appendAttempts()
            << " elapsed_us=" << sustainedUs << "\n";
  std::cout << "MEASURED sustained_mix=user_action,browser_request,request_result,state_change,noticeboard,browser_render\n";

  // Payload-pressure overload is a fault, not a silent truncation.
  MemoryDiagnosticStorage payloadMemory;
  InMemoryDiagnosticRecorder payloadRecorder(payloadMemory);
  const SessionStatus payloadStatus = startRecorder(payloadRecorder, 11, 7);
  std::vector<uint8_t> tooLarge(MemoryDiagnosticStorage::Phase2TestMaxPayloadBytes + 1, 0x44);
  uint64_t sequence = 0;
  const RecordView tooLargeRecord = makeRecord(payloadStatus, ClientId{7}, 1,
                                                tooLarge.data(), tooLarge.size());
  assert(payloadRecorder.tryAppend(tooLargeRecord, sequence) == RecorderResult::Fault);
  SessionStatus payloadFailure{};
  payloadRecorder.status(payloadFailure);
  assert(payloadFailure.storage == StorageHealth::Fault && payloadFailure.evidenceIncomplete);
  std::cout << "MEASURED payload_overload_bytes=" << tooLarge.size()
            << " result=FAULT dropped=" << payloadFailure.droppedRecords << "\n";

  // Retrieval timings for empty, small, near-capacity and moving views.
  MemoryDiagnosticStorage emptyMemory;
  InMemoryDiagnosticRecorder emptyRecorder(emptyMemory);
  startRecorder(emptyRecorder, 12, 1);
  size_t returned = 0, payloadBytes = 0;
  const uint64_t emptyUs = retrieveAll(emptyRecorder, returned, payloadBytes);
  std::cout << "MEASURED retrieval_empty_us=" << emptyUs << " records=" << returned
            << " payload_bytes=" << payloadBytes << "\n";

  MemoryDiagnosticStorage smallMemory;
  InMemoryDiagnosticRecorder smallRecorder(smallMemory);
  const SessionStatus smallStatus = startRecorder(smallRecorder, 13, 1);
  appendAccepted(smallRecorder, smallStatus, 1, small);
  appendAccepted(smallRecorder, smallStatus, 2, representative);
  const uint64_t smallUs = retrieveAll(smallRecorder, returned, payloadBytes);
  assert(returned == 2 && payloadBytes == 67);
  std::cout << "MEASURED retrieval_small_us=" << smallUs << " records=" << returned
            << " payload_bytes=" << payloadBytes << "\n";

  MemoryDiagnosticStorage nearMemory;
  InMemoryDiagnosticRecorder nearRecorder(nearMemory);
  const SessionStatus nearStatus = startRecorder(nearRecorder, 14, 1);
  for (uint32_t i = 1; i <= MemoryDiagnosticStorage::Phase2TestCapacity; ++i)
    appendAccepted(nearRecorder, nearStatus, i, maximum);
  const uint64_t nearUs = retrieveAll(nearRecorder, returned, payloadBytes);
  assert(returned == MemoryDiagnosticStorage::Phase2TestCapacity && payloadBytes ==
         MemoryDiagnosticStorage::Phase2TestCapacity * maximum.size());
  std::cout << "MEASURED retrieval_near_capacity_us=" << nearUs << " records=" << returned
            << " payload_bytes=" << payloadBytes << "\n";

  MemoryDiagnosticStorage movingMemory;
  InMemoryDiagnosticRecorder movingRecorder(movingMemory);
  const SessionStatus movingStatus = startRecorder(movingRecorder, 15, 1);
  appendAccepted(movingRecorder, movingStatus, 1, small);
  std::vector<uint8_t> movingPayload(MemoryDiagnosticStorage::Phase2TestMaxPayloadBytes, 0x55);
  size_t movingReturned = 0, movingBytes = 0;
  std::vector<uint8_t> retrievalPayload(DiagnosticRetrieval::MaxPayloadBytes);
  RetrievalChunk movingChunk{};
  assert(DiagnosticRetrieval::next(movingRecorder, 0, retrievalPayload.data(),
                                   retrievalPayload.size(), movingChunk));
  appendAccepted(movingRecorder, movingStatus, 2, movingPayload);
  assert(DiagnosticRetrieval::next(movingRecorder, movingChunk.nextCursor,
                                   retrievalPayload.data(), retrievalPayload.size(), movingChunk));
  if (movingChunk.available) { ++movingReturned; movingBytes += movingChunk.payloadBytes; }
  std::cout << "MEASURED retrieval_moving_records_after_append=" << movingReturned
            << " payload_bytes=" << movingBytes << "\n";

  measureFileRecord("stage15-resource-empty.bin", 0);
  measureFileRecord("stage15-resource-small.bin", small.size());
  measureFileRecord("stage15-resource-representative.bin", representative.size());
  measureFileRecord("stage15-resource-file-max.bin", DevelopmentFileStorage::DevelopmentFileMaxPayloadBytes);
  measureFileScaling("stage15-resource-scale-1.bin", 1, representative.size());
  measureFileScaling("stage15-resource-scale-8.bin", 8, representative.size());
  measureFileScaling("stage15-resource-scale-64.bin", DevelopmentFileStorage::DevelopmentFileMaxRecords,
                     representative.size());

  // Failure injection is deterministic and latches without retrying.
  MemoryDiagnosticStorage failingMemory;
  FailureInjectionStorage injected(failingMemory);
  InMemoryDiagnosticRecorder failingRecorder(injected);
  const SessionStatus failingStatus = startRecorder(failingRecorder, 16, 1);
  injected.failNextAppend(StorageResult::WriteFailed);
  const RecordView failureRecord = makeRecord(failingStatus, ClientId{1}, 1,
                                               small.data(), small.size());
  assert(failingRecorder.tryAppend(failureRecord, sequence) == RecorderResult::Fault);
  const uint64_t callsAfterFailure = failingMemory.appendAttempts();
  assert(failingRecorder.tryAppend(failureRecord, sequence) == RecorderResult::Fault);
  assert(failingMemory.appendAttempts() == callsAfterFailure);
  SessionStatus failureStatus{};
  failingRecorder.status(failureStatus);
  assert(failureStatus.storage == StorageHealth::Fault && failureStatus.evidenceIncomplete);
  std::cout << "MEASURED append_failure backend_calls=" << callsAfterFailure
            << " retry_backend_calls=" << failingMemory.appendAttempts()
            << " health=FAULT evidence_incomplete="
            << (failureStatus.evidenceIncomplete ? 1 : 0) << "\n";

  std::cout << "MEASURED host_campaign=PASS\n";
  return 0;
}
