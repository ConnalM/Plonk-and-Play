#include "pp/diagnostic_export.h"
#include "pp/diagnostic_production.h"

#include <cstring>
#include <string>

using namespace pp::diagnostic;

class StringSink final : public DiagnosticExportSink {
public:
  bool write(const char* data, size_t length) override {
    value.append(data, length);
    return true;
  }
  std::string value;
};

class ReadFailureStorage final : public DiagnosticStorage {
public:
  StorageResult begin(ControllerEpoch) override { open_ = true; return StorageResult::Accepted; }
  StorageResult tryAppend(const RecordView&, uint64_t) override { return StorageResult::Accepted; }
  size_t count() const override { return open_ ? 2 : 0; }
  bool read(size_t, RecordEnvelope&, uint8_t*, size_t, size_t& length) const override {
    length = 0;
    return false;
  }
  StorageStatus status() const override {
    return {StorageHealth::RamOnly, BackendKind::FailureInjection,
            FailureReason::None, false};
  }
  StorageResult recover() override { return StorageResult::Accepted; }
  StorageResult close() override { open_ = false; return StorageResult::Accepted; }
private:
  bool open_ = false;
};

class SequentialReadStorage final : public DiagnosticStorage {
public:
  static constexpr size_t Capacity = 8;

  StorageResult begin(ControllerEpoch) override {
    open_ = true;
    return StorageResult::Accepted;
  }
  StorageResult tryAppend(const RecordView& record, uint64_t sequence) override {
    if (!open_ || count_ == Capacity) return StorageResult::Full;
    items_[count_].envelope = record.envelope;
    items_[count_].envelope.sequence = sequence;
    items_[count_].length = record.payloadBytes;
    if (record.payloadBytes) std::memcpy(items_[count_].payload, record.payload,
                                         record.payloadBytes);
    ++count_;
    ++revision_;
    return StorageResult::Accepted;
  }
  size_t count() const override { return count_; }
  uint64_t revision() const override { return revision_; }
  bool read(size_t index, RecordEnvelope& envelope, uint8_t* payload,
            size_t capacity, size_t& length) const override {
    ++readCalls;
    length = 0;
    if (index >= count_ || items_[index].length > capacity) return false;
    envelope = items_[index].envelope;
    if (items_[index].length) std::memcpy(payload, items_[index].payload,
                                           items_[index].length);
    length = items_[index].length;
    return true;
  }
  StorageStatus status() const override {
    return {StorageHealth::DurableSd, BackendKind::ProductionSd,
            FailureReason::None, true};
  }
  StorageResult recover() override { return StorageResult::Accepted; }
  StorageResult close() override { open_ = false; return StorageResult::Accepted; }

  mutable size_t readCalls = 0;

private:
  struct Item {
    RecordEnvelope envelope{};
    uint8_t payload[DiagnosticExport::MaxPayloadBytes]{};
    size_t length = 0;
  };
  Item items_[Capacity]{};
  size_t count_ = 0;
  uint64_t revision_ = 0;
  bool open_ = false;
};

static RecordView makeRecord(const SessionStatus& session, uint32_t value,
                             uint8_t* payload) {
  payload[0] = static_cast<uint8_t>(value);
  payload[1] = static_cast<uint8_t>(value >> 8);
  RecordView record{};
  record.envelope.controllerEpoch = session.controllerEpoch;
  record.envelope.session = session.session;
  record.envelope.client.value = 7;
  record.envelope.sequence = value;
  record.envelope.timestamp = value * 10;
  record.envelope.correlation.request = value;
  record.envelope.kind = EventKind::BrowserRequest;
  record.payload = payload;
  record.payloadBytes = 2;
  return record;
}

static DiagnosticExportMetadata metadata(const SessionStatus& normal,
                                         const StorageStatus& storage,
                                         const char* capability) {
  DiagnosticExportMetadata value{};
  value.firmwareBuild = "TEST_BUILD_15_EXPORT";
  value.backend = storage.backend == BackendKind::FailureInjection
                      ? "FAILURE_INJECTION" : "RAM";
  value.capability = capability;
  value.storageHealth = static_cast<uint8_t>(storage.health);
  value.controllerEpoch = normal.controllerEpoch;
  value.normalSession = normal.session;
  value.evidenceVolatile = true;
  return value;
}

int main() {
  ControllerEpoch epoch{77};
  ClientId client{1};

  // Empty export is a valid, bounded artifact.
  ProductionDiagnosticStorage emptyStorage;
  ProductionDiagnosticRecorder empty(emptyStorage);
  if (!empty.beginNormal(epoch, client)) return 1;
  SessionStatus emptyStatus{};
  empty.status(emptyStatus);
  const StorageStatus emptyStorageStatus = emptyStorage.status();
  StringSink emptySink;
  DiagnosticExportResult emptyResult{};
  const char* emptyCapability = DiagnosticExport::capability(emptyStorageStatus, empty.count());
  if (!DiagnosticExport::write(empty, metadata(emptyStatus, emptyStorageStatus, emptyCapability),
                               0, emptySink, emptyResult)) return 2;
  if (!emptyResult.complete || emptyResult.snapshotRecords != 0 ||
      emptyResult.exportedRecords != 0 ||
      emptySink.value.find("\"type\":\"summary\"") == std::string::npos) return 3;

  // Non-empty export uses the same encoder called by /diag/download.
  ProductionDiagnosticStorage storage;
  ProductionDiagnosticRecorder recorder(storage);
  if (!recorder.beginNormal(epoch, client)) return 4;
  SessionStatus normal{};
  recorder.status(normal);
  uint8_t payload[2]{};
  for (uint32_t value = 1; value <= 3; ++value) {
    uint64_t sequence = 0;
    if (recorder.tryAppend(makeRecord(normal, value, payload), sequence) != RecorderResult::Accepted)
      return 5;
  }
  const StorageStatus storageStatus = storage.status();
  StringSink sink;
  DiagnosticExportResult result{};
  const char* capability = DiagnosticExport::capability(storageStatus, recorder.count());
  if (strcmp(capability, "LIMITED") != 0) return 6;
  if (!DiagnosticExport::write(recorder, metadata(normal, storageStatus, capability),
                               0, sink, result)) return 7;
  if (!result.complete || result.snapshotRecords != 3 || result.exportedRecords != 3 ||
      sink.value.find("TEST_BUILD_15_EXPORT") == std::string::npos ||
      sink.value.find("\"backend\":\"RAM\"") == std::string::npos ||
      sink.value.find("\"sequence\":1") == std::string::npos ||
      sink.value.find("\"sequence\":2") == std::string::npos ||
      sink.value.find("\"sequence\":3") == std::string::npos ||
      sink.value.find("\"records\":3") == std::string::npos) return 8;
  for (const char* forbidden : {"password", "secret", "token", "Cookie", "privateKey"})
    if (sink.value.find(forbidden) != std::string::npos) return 8;
  if (sink.value.find("\"sequence\":2") < sink.value.find("\"sequence\":1") ||
      sink.value.find("\"sequence\":3") < sink.value.find("\"sequence\":2")) return 9;

  // Export must read each chronological record once. The old implementation
  // reread every earlier record while checking for duplicates, making an
  // SD-backed export quadratic in record count.
  SequentialReadStorage sequentialStorage;
  ProductionDiagnosticRecorder sequentialRecorder(sequentialStorage);
  if (!sequentialRecorder.beginNormal(epoch, client)) return 16;
  SessionStatus sequentialStatus{};
  sequentialRecorder.status(sequentialStatus);
  uint64_t sequentialSequence = 0;
  for (uint32_t value = 1; value <= SequentialReadStorage::Capacity; ++value) {
    if (sequentialRecorder.tryAppend(makeRecord(sequentialStatus, value, payload), sequentialSequence) !=
        RecorderResult::Accepted) return 17;
  }
  StringSink sequentialSink;
  DiagnosticExportResult sequentialResult{};
  if (!DiagnosticExport::write(sequentialRecorder,
                               metadata(sequentialStatus, sequentialStorage.status(),
                                        "AVAILABLE"),
                               0, sequentialSink, sequentialResult) ||
      !sequentialResult.complete ||
      sequentialStorage.readCalls != sequentialResult.snapshotRecords) return 18;

  // A nonzero snapshot whose first read fails must produce no false complete
  // export and must still return a deterministic diagnostic summary.
  ReadFailureStorage failingStorage;
  ProductionDiagnosticRecorder failing(failingStorage);
  if (!failing.beginNormal(epoch, client)) return 10;
  SessionStatus failingStatus{};
  failing.status(failingStatus);
  const StorageStatus failingStatusStore = failingStorage.status();
  StringSink failingSink;
  DiagnosticExportResult failingResult{};
  const char* failingCapability = DiagnosticExport::capability(
      failingStatusStore, failing.count());
  if (strcmp(failingCapability, "LIMITED") != 0) return 11;
  if (!DiagnosticExport::write(failing,
                               metadata(failingStatus, failingStatusStore, failingCapability),
                               0, failingSink, failingResult)) return 12;
  if (failingResult.snapshotRecords != 2 || failingResult.exportedRecords != 0 ||
      failingResult.complete || !failingResult.readFailed ||
      failingSink.value.find("\"type\":\"record\"") != std::string::npos ||
      failingSink.value.find("\"readFailed\":true") == std::string::npos ||
      failingSink.value.find("\"evidenceIncomplete\":true") == std::string::npos) return 13;

  // The capability contract is independent of record count.
  StorageStatus durable{StorageHealth::DurableSd, BackendKind::ProductionSd,
                        FailureReason::None, true};
  if (strcmp(DiagnosticExport::capability(durable, 139), "AVAILABLE") != 0) return 14;
  StorageStatus fault{StorageHealth::Fault, BackendKind::ProductionSd,
                      FailureReason::BackendUnavailable, false};
  if (strcmp(DiagnosticExport::capability(fault, 0), "UNAVAILABLE") != 0) return 15;
  return 0;
}
