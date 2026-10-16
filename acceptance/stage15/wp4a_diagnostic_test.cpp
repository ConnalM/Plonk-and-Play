#include "pp/diagnostic_production.h"

#include <vector>

using namespace pp::diagnostic;

class DurableTestStorage final : public DiagnosticStorage {
public:
  StorageResult begin(ControllerEpoch) override { open = true; health = StorageHealth::DurableSd; return StorageResult::Accepted; }
  StorageResult tryAppend(const RecordView& record, uint64_t sequence) override {
    if (!open) return StorageResult::Unavailable;
    if (failWrites) { health = StorageHealth::Fault; return StorageResult::WriteFailed; }
    Item item{}; item.envelope = record.envelope; item.envelope.sequence = sequence;
    item.payload.assign(record.payload, record.payload + record.payloadBytes); items.push_back(item); last = sequence; return StorageResult::Accepted;
  }
  size_t count() const override { return items.size(); }
  bool read(size_t index, RecordEnvelope& envelope, uint8_t* payload, size_t capacity, size_t& length) const override {
    if (index >= items.size() || items[index].payload.size() > capacity) return false;
    envelope = items[index].envelope; length = items[index].payload.size();
    if (length) memcpy(payload, items[index].payload.data(), length);
    return true;
  }
  StorageStatus status() const override { return {health, BackendKind::ProductionSd, FailureReason::None, health == StorageHealth::DurableSd}; }
  StorageResult recover() override { if (corrupt) { health = StorageHealth::Degraded; return StorageResult::Corrupt; } health = StorageHealth::DurableSd; return StorageResult::Accepted; }
  StorageResult close() override { open = false; return StorageResult::Accepted; }
  uint64_t lastSequence() const override { return last; }
  struct Item { RecordEnvelope envelope{}; std::vector<uint8_t> payload; };
  std::vector<Item> items; bool open = false; bool failWrites = false; bool corrupt = false; StorageHealth health = StorageHealth::Fault; uint64_t last = 0;
};

static RecordView record(const SessionStatus& session, uint32_t value) {
  static uint8_t payload[2]; payload[0] = uint8_t(value); payload[1] = uint8_t(value >> 8);
  RecordView result{}; result.envelope.controllerEpoch = session.controllerEpoch; result.envelope.session = session.session;
  result.envelope.client.value = 7; result.envelope.correlation.request = value; result.payload = payload; result.payloadBytes = sizeof(payload); return result;
}

int main() {
  DurableTestStorage storage;
  ControllerEpoch epoch{42}; ClientId client{1};
  ProductionDiagnosticRecorder first(storage);
  if (!first.beginNormal(epoch, client)) return 1;
  SessionStatus normal{}; if (!first.status(normal) || normal.storage != StorageHealth::DurableSd) return 2;
  uint64_t sequence = 0;
  if (first.tryAppend(record(normal, 1), sequence) != RecorderResult::Accepted || sequence != 1) return 3;
  SessionStatus development{}; SessionStartRequest request{epoch, client};
  if (first.startDevelopment(request, development) != RecorderResult::Accepted) return 4;
  if (first.tryAppend(record(development, 2), sequence) != RecorderResult::Accepted || sequence != 2) return 5;
  first.tick(1); first.tick(ProductionDiagnosticRecorder::DevelopmentLimitUs + 1);
  if (!first.developmentStatus(development) || development.lifecycle != SessionLifecycle::Completed) return 6;

  // A new controller object recovers durable evidence, but never resumes
  // volatile Development mode.
  ProductionDiagnosticRecorder restarted(storage);
  if (!restarted.beginNormal(epoch, client) || restarted.count() != 2) return 7;
  if (!restarted.status(normal) || normal.nextSequence != 2 || normal.lifecycle != SessionLifecycle::Active) return 8;
  if (!restarted.developmentStatus(development) || development.lifecycle != SessionLifecycle::Disabled) return 9;

  storage.failWrites = true; if (restarted.tryAppend(record(normal, 3), sequence) != RecorderResult::Fault) return 10;
  if (!restarted.status(normal) || normal.storage != StorageHealth::Fault || !normal.evidenceIncomplete) return 11;
  storage.failWrites = false; storage.corrupt = true; if (storage.recover() != StorageResult::Corrupt) return 12;
  return 0;
}
