#include "pp/diagnostic_export.h"
#include "pp/diagnostic_production.h"

#include <cstring>
#include <string>

using namespace pp::diagnostic;

class DurableSequenceStorage final : public DiagnosticStorage {
public:
  static constexpr size_t Capacity = 3;

  StorageResult begin(ControllerEpoch) override {
    open_ = true;
    health_ = StorageHealth::DurableSd;
    lastFailure_ = FailureReason::None;
    return StorageResult::Accepted;
  }

  StorageResult tryAppend(const RecordView& record, uint64_t sequence) override {
    if (!open_) return StorageResult::Unavailable;
    if (count_ >= Capacity) {
      health_ = StorageHealth::Degraded;
      lastFailure_ = FailureReason::StorageFull;
      return StorageResult::Full;
    }
    Item& item = items_[count_++];
    item.envelope = record.envelope;
    item.envelope.sequence = sequence;
    item.length = record.payloadBytes;
    if (item.length) std::memcpy(item.payload, record.payload, item.length);
    if (sequence > lastSequence_) lastSequence_ = sequence;
    ++revision_;
    if (ambiguousNext_) {
      ambiguousNext_ = false;
      health_ = StorageHealth::Fault;
      lastFailure_ = FailureReason::WriteFailure;
      return StorageResult::WriteFailed;
    }
    health_ = StorageHealth::DurableSd;
    lastFailure_ = FailureReason::None;
    return StorageResult::Accepted;
  }

  size_t count() const override { return count_; }
  uint64_t revision() const override { return revision_; }
  uint64_t lastSequence() const override { return lastSequence_; }

  bool read(size_t index, RecordEnvelope& envelope, uint8_t* payload,
            size_t capacity, size_t& length) const override {
    length = 0;
    if (index >= count_) return false;
    const Item& item = items_[index];
    if (item.length > capacity || (item.length && !payload)) return false;
    envelope = item.envelope;
    if (item.length) std::memcpy(payload, item.payload, item.length);
    length = item.length;
    return true;
  }

  StorageStatus status() const override {
    return {health_, BackendKind::ProductionSd, lastFailure_, true};
  }

  StorageResult recover() override {
    open_ = true;
    health_ = StorageHealth::DurableSd;
    lastFailure_ = FailureReason::None;
    return StorageResult::Accepted;
  }

  StorageResult close() override {
    open_ = false;
    return StorageResult::Accepted;
  }

  bool evictOldest() override {
    if (!count_) return false;
    for (size_t i = 1; i < count_; ++i) items_[i - 1] = items_[i];
    --count_;
    ++revision_;
    health_ = StorageHealth::Degraded;
    lastFailure_ = FailureReason::StorageFull;
    return true;
  }

  void failNextAppendAfterCommit() { ambiguousNext_ = true; }

  void seed(uint64_t sequence, ControllerEpoch epoch) {
    RecordView record{};
    record.envelope.controllerEpoch = epoch;
    record.envelope.session.value = 1;
    record.envelope.client.value = 1;
    record.envelope.kind = EventKind::StateChange;
    uint8_t payload = static_cast<uint8_t>(sequence);
    record.payload = &payload;
    record.payloadBytes = 1;
    (void)tryAppend(record, sequence);
  }

private:
  struct Item {
    RecordEnvelope envelope{};
    uint8_t payload[DiagnosticExport::MaxPayloadBytes]{};
    size_t length = 0;
  };

  Item items_[Capacity]{};
  size_t count_ = 0;
  uint64_t lastSequence_ = 0;
  uint64_t revision_ = 0;
  StorageHealth health_ = StorageHealth::Fault;
  FailureReason lastFailure_ = FailureReason::None;
  bool open_ = false;
  bool ambiguousNext_ = false;
};

static RecordView makeRecord(const SessionStatus& status, uint8_t value) {
  static uint8_t payload[1];
  payload[0] = value;
  RecordView record{};
  record.envelope.controllerEpoch = status.controllerEpoch;
  record.envelope.session = status.session;
  record.envelope.client.value = 7;
  record.envelope.kind = EventKind::BrowserRequest;
  record.payload = payload;
  record.payloadBytes = sizeof(payload);
  return record;
}

class StringSink final : public DiagnosticExportSink {
public:
  bool write(const char* data, size_t length) override {
    value.append(data, length);
    return true;
  }
  std::string value;
};

static DiagnosticExportMetadata metadata(const SessionStatus& status) {
  DiagnosticExportMetadata value{};
  value.firmwareBuild = "SEQUENCE_TEST";
  value.backend = "ESP32_SPI_SD";
  value.capability = "AVAILABLE";
  value.storageHealth = static_cast<uint8_t>(StorageHealth::DurableSd);
  value.controllerEpoch = status.controllerEpoch;
  value.normalSession = status.session;
  return value;
}

int main() {
  DurableSequenceStorage storage;
  ControllerEpoch epoch1{100};
  ClientId client{1};
  ProductionDiagnosticRecorder first(storage);
  if (!first.beginNormal(epoch1, client)) return 1;
  SessionStatus status{};

  uint64_t sequence = 0;
  if (first.tryAppend(makeRecord(status, 1), sequence) != RecorderResult::Accepted ||
      sequence != 1) return 2;
  storage.failNextAppendAfterCommit();
  if (first.tryAppend(makeRecord(status, 2), sequence) != RecorderResult::Fault ||
      storage.count() != 2) return 3;
  if (first.tryAppend(makeRecord(status, 3), sequence) != RecorderResult::Accepted ||
      sequence != 3) return 4;

  // A reboot restores the durable high-water mark and continues above it in
  // a new controller epoch. No sequence is reused across epochs.
  ControllerEpoch epoch2{200};
  ProductionDiagnosticRecorder rebooted(storage);
  if (!rebooted.beginNormal(epoch2, client) ||
      rebooted.tryAppend(makeRecord(status, 4), sequence) != RecorderResult::Accepted ||
      sequence != 4) return 5;

  // Rolling retention evicts old evidence but never rewinds the sequence.
  if (rebooted.tryAppend(makeRecord(status, 5), sequence) != RecorderResult::Accepted ||
      sequence != 5 || storage.count() != DurableSequenceStorage::Capacity) return 6;
  RecordEnvelope oldest{};
  uint8_t payload[DiagnosticExport::MaxPayloadBytes]{};
  size_t length = 0;
  if (!rebooted.read(0, oldest, payload, sizeof(payload), length) ||
      oldest.sequence != 3) return 7;

  SessionStatus current{};
  rebooted.status(current);
  StringSink cleanSink;
  DiagnosticExportResult cleanResult{};
  if (!DiagnosticExport::write(rebooted, metadata(current), 1, cleanSink, cleanResult) ||
      !cleanResult.complete || cleanResult.duplicateSequence ||
      cleanResult.exportedRecords != cleanResult.snapshotRecords ||
      cleanSink.value.find("\"duplicateSequence\":false") == std::string::npos) return 8;

  // Historical duplicates are not rewritten or hidden. They remain readable,
  // but a complete export reports the anomaly explicitly.
  DurableSequenceStorage legacy;
  legacy.begin(ControllerEpoch{300});
  legacy.seed(7, ControllerEpoch{300});
  legacy.seed(7, ControllerEpoch{300});
  ProductionDiagnosticRecorder legacyRecorder(legacy);
  if (!legacyRecorder.beginNormal(ControllerEpoch{301}, client)) return 9;
  SessionStatus legacyStatus{};
  legacyRecorder.status(legacyStatus);
  StringSink legacySink;
  DiagnosticExportResult legacyResult{};
  if (!DiagnosticExport::write(legacyRecorder, metadata(legacyStatus), 0,
                               legacySink, legacyResult) ||
      legacyResult.exportedRecords != 2 || legacyResult.complete ||
      !legacyResult.duplicateSequence ||
      legacySink.value.find("\"duplicateSequence\":true") == std::string::npos) return 10;
  return 0;
}
