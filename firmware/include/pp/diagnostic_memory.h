#pragma once

#include <string.h>

#include "diagnostic_contract.h"

namespace pp {
namespace diagnostic {

// These bounds exist only to exercise Phase 2 on a host and in development
// tests. They are deliberately not product capacities and must not be reused
// as the eventual SD retention policy.
class MemoryDiagnosticStorage final : public DiagnosticStorage {
public:
  static constexpr size_t Phase2TestCapacity = 8;
  static constexpr size_t Phase2TestMaxPayloadBytes = 128;

  StorageResult begin(ControllerEpoch controllerEpoch) override {
    if (!controllerEpoch.valid()) {
      health_ = StorageHealth::Fault;
      lastFailure_ = FailureReason::InvalidRequest;
      return StorageResult::Invalid;
    }
    epoch_ = controllerEpoch;
    open_ = true;
    health_ = StorageHealth::RamOnly;
    lastFailure_ = FailureReason::None;
    return StorageResult::Accepted;
  }

  StorageResult tryAppend(const RecordView& record, uint64_t sequence) override {
    ++appendAttempts_;
    if (!open_) {
      health_ = StorageHealth::Fault;
      lastFailure_ = FailureReason::BackendUnavailable;
      return StorageResult::Unavailable;
    }
    if (nextResult_ != StorageResult::Accepted) {
      const StorageResult result = nextResult_;
      nextResult_ = StorageResult::Accepted;
      health_ = StorageHealth::Fault;
      lastFailure_ = reasonFor(result);
      return result;
    }
    if (count_ >= Phase2TestCapacity) {
      health_ = StorageHealth::Degraded;
      lastFailure_ = FailureReason::StorageFull;
      return StorageResult::Full;
    }
    if (record.payloadBytes > Phase2TestMaxPayloadBytes ||
        (record.payloadBytes != 0 && !record.payload)) {
      health_ = StorageHealth::Fault;
      lastFailure_ = FailureReason::PayloadFailure;
      return StorageResult::Invalid;
    }
    StoredRecord& stored = records_[count_];
    stored.envelope = record.envelope;
    stored.envelope.sequence = sequence;
    stored.payloadBytes = record.payloadBytes;
    if (record.payloadBytes) memcpy(stored.payload, record.payload, record.payloadBytes);
    ++count_;
    return StorageResult::Accepted;
  }

  size_t count() const override { return count_; }

  bool read(size_t index, RecordEnvelope& envelope, uint8_t* payload,
            size_t capacity, size_t& length) const override {
    length = 0;
    if (index >= count_) return false;
    const StoredRecord& stored = records_[index];
    if (stored.payloadBytes > capacity || (stored.payloadBytes != 0 && !payload)) return false;
    envelope = stored.envelope;
    if (stored.payloadBytes) memcpy(payload, stored.payload, stored.payloadBytes);
    length = stored.payloadBytes;
    return true;
  }

  StorageStatus status() const override {
    StorageStatus result{};
    result.health = health_;
    result.lastFailure = lastFailure_;
    result.durable = false;
    return result;
  }

  StorageResult recover() override {
    if (!open_) return StorageResult::Unavailable;
    nextResult_ = StorageResult::Accepted;
    health_ = StorageHealth::RamOnly;
    lastFailure_ = FailureReason::None;
    return StorageResult::Accepted;
  }

  StorageResult close() override {
    open_ = false;
    return StorageResult::Accepted;
  }

  void failNextAppend(StorageResult result) { nextResult_ = result; }
  uint64_t appendAttempts() const { return appendAttempts_; }
  ControllerEpoch epoch() const { return epoch_; }

private:
  struct StoredRecord {
    RecordEnvelope envelope{};
    uint8_t payload[Phase2TestMaxPayloadBytes]{};
    size_t payloadBytes = 0;
  };

  static FailureReason reasonFor(StorageResult result) {
    switch (result) {
      case StorageResult::Unavailable: return FailureReason::BackendUnavailable;
      case StorageResult::Full: return FailureReason::StorageFull;
      case StorageResult::WriteFailed: return FailureReason::WriteFailure;
      case StorageResult::CommitFailed: return FailureReason::CommitFailure;
      case StorageResult::Corrupt: return FailureReason::CorruptRecord;
      case StorageResult::UnsupportedVersion: return FailureReason::UnsupportedVersion;
      case StorageResult::Invalid: return FailureReason::InvalidRequest;
      default: return FailureReason::RecorderFault;
    }
  }

  StoredRecord records_[Phase2TestCapacity]{};
  ControllerEpoch epoch_{};
  StorageResult nextResult_ = StorageResult::Accepted;
  StorageHealth health_ = StorageHealth::Fault;
  FailureReason lastFailure_ = FailureReason::None;
  size_t count_ = 0;
  uint64_t appendAttempts_ = 0;
  bool open_ = false;
};

class InMemoryDiagnosticRecorder final : public DiagnosticRecorder {
public:
  explicit InMemoryDiagnosticRecorder(DiagnosticStorage& storage) : storage_(storage) {}

  RecorderResult start(const SessionStartRequest& request, SessionStatus& status) override {
    if (isActive()) {
      status = status_;
      return RecorderResult::AlreadyActive;
    }
    if (!request.controllerEpoch.valid() || !request.client.valid()) {
      status_.lastFailure = FailureReason::InvalidRequest;
      status = status_;
      return RecorderResult::Invalid;
    }
    status_ = SessionStatus{};
    status_.controllerEpoch = request.controllerEpoch;
    status_.session.value = ++nextSessionId_;
    status_.lifecycle = SessionLifecycle::Starting;
    const StorageResult result = storage_.begin(request.controllerEpoch);
    if (result != StorageResult::Accepted) {
      // A development recorder may continue in RAM-only/degraded mode when
      // durable storage is unavailable; this backend failure is still visible.
      status_.storage = StorageHealth::Degraded;
      status_.evidenceVolatile = true;
      status_.evidenceIncomplete = true;
      status_.lastFailure = FailureReason::BackendUnavailable;
    } else {
      status_.storage = storage_.status().health;
      status_.evidenceVolatile = status_.storage != StorageHealth::DurableSd;
    }
    status_.lifecycle = SessionLifecycle::Active;
    faultLatched_ = false;
    fullLatched_ = false;
    status = status_;
    return RecorderResult::Accepted;
  }

  RecorderResult stop(SessionStatus& status) override {
    if (!isActive()) { status = status_; return RecorderResult::NotActive; }
    status_.lifecycle = SessionLifecycle::Stopping;
    const StorageResult result = storage_.close();
    if (result != StorageResult::Accepted) {
      latchFault(FailureReason::CommitFailure);
      status_.lifecycle = SessionLifecycle::Interrupted;
      status = status_;
      return RecorderResult::Fault;
    }
    status_.lifecycle = SessionLifecycle::Completed;
    status = status_;
    return RecorderResult::Accepted;
  }

  RecorderResult interrupt(SessionStatus& status) override {
    if (!isActive()) { status = status_; return RecorderResult::NotActive; }
    storage_.close();
    status_.lifecycle = SessionLifecycle::Interrupted;
    status_.evidenceIncomplete = true;
    status = status_;
    return RecorderResult::Accepted;
  }

  RecorderResult tryAppend(const RecordView& record, uint64_t& sequence) override {
    sequence = 0;
    if (!isActive()) return RecorderResult::NotActive;
    if (faultLatched_) { ++status_.droppedRecords; status_.evidenceIncomplete = true; return RecorderResult::Fault; }
    if (fullLatched_) { ++status_.droppedRecords; status_.evidenceIncomplete = true; return RecorderResult::Dropped; }
    if (record.envelope.controllerEpoch != status_.controllerEpoch ||
        record.envelope.session.value != status_.session.value ||
        record.envelope.formatVersion != RecordEnvelope::CurrentFormatVersion) {
      status_.lastFailure = FailureReason::InvalidRequest;
      return RecorderResult::Invalid;
    }
    const uint64_t candidate = status_.nextSequence + 1;
    const StorageResult result = storage_.tryAppend(record, candidate);
    if (result == StorageResult::Accepted) {
      status_.nextSequence = candidate;
      sequence = candidate;
      return RecorderResult::Accepted;
    }
    if (result == StorageResult::Full) {
      fullLatched_ = true;
      status_.storage = StorageHealth::Degraded;
      status_.lastFailure = FailureReason::StorageFull;
      status_.evidenceIncomplete = true;
      ++status_.droppedRecords;
      return RecorderResult::Dropped;
    }
    latchFault(reasonFor(result));
    ++status_.droppedRecords;
    return RecorderResult::Fault;
  }

  bool status(SessionStatus& status) const override { status = status_; return true; }

  size_t count() const override { return storage_.count(); }

  bool read(size_t index, RecordEnvelope& envelope, uint8_t* payload,
            size_t capacity, size_t& length) const override {
    return storage_.read(index, envelope, payload, capacity, length);
  }

private:
  bool isActive() const {
    return status_.lifecycle == SessionLifecycle::Starting ||
           status_.lifecycle == SessionLifecycle::Active ||
           status_.lifecycle == SessionLifecycle::Stopping;
  }

  static FailureReason reasonFor(StorageResult result) {
    switch (result) {
      case StorageResult::Unavailable: return FailureReason::BackendUnavailable;
      case StorageResult::Full: return FailureReason::StorageFull;
      case StorageResult::WriteFailed: return FailureReason::WriteFailure;
      case StorageResult::CommitFailed: return FailureReason::CommitFailure;
      case StorageResult::Corrupt: return FailureReason::CorruptRecord;
      case StorageResult::UnsupportedVersion: return FailureReason::UnsupportedVersion;
      default: return FailureReason::RecorderFault;
    }
  }

  void latchFault(FailureReason reason) {
    faultLatched_ = true;
    status_.storage = StorageHealth::Fault;
    status_.lastFailure = reason;
    status_.evidenceIncomplete = true;
  }

  DiagnosticStorage& storage_;
  SessionStatus status_{};
  uint64_t nextSessionId_ = 0;
  bool faultLatched_ = false;
  bool fullLatched_ = false;
};

} // namespace diagnostic
} // namespace pp

