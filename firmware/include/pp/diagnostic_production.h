#pragma once

#include <string.h>

#include "diagnostic_contract.h"

namespace pp {
namespace diagnostic {

// Provisional production-development bounds.  These are intentionally named
// implementation bounds; they are not the final SD retention policy.
class ProductionDiagnosticStorage final : public DiagnosticStorage {
public:
  static constexpr size_t Capacity = 64;
  static constexpr size_t MaxPayloadBytes = 256;

  StorageResult begin(ControllerEpoch epoch) override {
    if (!epoch.valid()) return fail(StorageResult::Invalid, FailureReason::InvalidRequest);
    epoch_ = epoch;
    count_ = 0;
    bytes_ = 0;
    revision_ = 0;
    open_ = true;
    health_ = StorageHealth::RamOnly;
    lastFailure_ = FailureReason::None;
    return StorageResult::Accepted;
  }

  StorageResult tryAppend(const RecordView& record, uint64_t sequence) override {
    if (!open_) return fail(StorageResult::Unavailable, FailureReason::BackendUnavailable);
    if (count_ >= Capacity) return fail(StorageResult::Full, FailureReason::StorageFull);
    if (record.payloadBytes > MaxPayloadBytes || (record.payloadBytes && !record.payload))
      return fail(StorageResult::Invalid, FailureReason::PayloadFailure);
    Stored& value = records_[count_];
    value.envelope = record.envelope;
    value.envelope.sequence = sequence;
    value.payloadBytes = record.payloadBytes;
    if (record.payloadBytes) memcpy(value.payload, record.payload, record.payloadBytes);
    ++count_;
    bytes_ += record.payloadBytes;
    ++revision_;
    return StorageResult::Accepted;
  }

  size_t count() const override { return count_; }
  uint64_t revision() const override { return revision_; }
  size_t bytes() const { return bytes_; }

  bool evictOldest() override {
    if (count_ == 0) return false;
    bytes_ -= records_[0].payloadBytes;
    if (count_ > 1) memmove(records_, records_ + 1, sizeof(Stored) * (count_ - 1));
    --count_;
    ++revision_;
    health_ = StorageHealth::Degraded;
    lastFailure_ = FailureReason::StorageFull;
    return true;
  }

  bool read(size_t index, RecordEnvelope& envelope, uint8_t* payload,
            size_t capacity, size_t& length) const override {
    length = 0;
    if (index >= count_) return false;
    const Stored& value = records_[index];
    if (value.payloadBytes > capacity || (value.payloadBytes && !payload)) return false;
    envelope = value.envelope;
    if (value.payloadBytes) memcpy(payload, value.payload, value.payloadBytes);
    length = value.payloadBytes;
    return true;
  }

  StorageStatus status() const override {
    StorageStatus value{};
    value.health = health_;
    value.backend = BackendKind::Memory;
    value.lastFailure = lastFailure_;
    value.durable = false;
    return value;
  }

  StorageResult recover() override {
    if (!open_) return StorageResult::Unavailable;
    health_ = StorageHealth::RamOnly;
    lastFailure_ = FailureReason::None;
    return StorageResult::Accepted;
  }

  StorageResult close() override {
    open_ = false;
    return StorageResult::Accepted;
  }

private:
  StorageResult fail(StorageResult result, FailureReason reason) {
    if (result != StorageResult::Full) health_ = StorageHealth::Fault;
    else health_ = StorageHealth::Degraded;
    lastFailure_ = reason;
    return result;
  }

  struct Stored {
    RecordEnvelope envelope{};
    uint8_t payload[MaxPayloadBytes]{};
    size_t payloadBytes = 0;
  };

  Stored records_[Capacity]{};
  ControllerEpoch epoch_{};
  StorageHealth health_ = StorageHealth::Fault;
  FailureReason lastFailure_ = FailureReason::None;
  size_t count_ = 0;
  size_t bytes_ = 0;
  uint64_t revision_ = 0;
  bool open_ = false;
};

// One recorder owns the always-on Normal stream and a separate volatile
// Development mode.  Both streams use the same bounded observation store so
// expiry never deletes evidence already recorded and reboot cannot resume the
// Development mode implicitly.
class ProductionDiagnosticRecorder final : public DiagnosticRecorder {
public:
  static constexpr Time DevelopmentLimitUs = Time(4) * 60 * 60 * 1000000ULL;

  explicit ProductionDiagnosticRecorder(DiagnosticStorage& storage) : storage_(storage) {}

  bool beginNormal(ControllerEpoch epoch, ClientId client) {
    if (!epoch.valid() || !client.valid()) return false;
    normal_ = SessionStatus{};
    normal_.controllerEpoch = epoch;
    normal_.session.value = 1;
    normal_.lifecycle = SessionLifecycle::Active;
    normal_.evidenceVolatile = true;
    normal_.storage = StorageHealth::RamOnly;
    normalClient_ = client;
    const StorageResult result = storage_.begin(epoch);
    if (result != StorageResult::Accepted) {
      normal_.storage = StorageHealth::Degraded;
      normal_.evidenceIncomplete = true;
      normal_.lastFailure = FailureReason::BackendUnavailable;
      return false;
    }
    nextSequence_ = storage_.lastSequence();
    normal_.nextSequence = nextSequence_;
    const StorageStatus storageStatus = storage_.status();
    normal_.storage = storageStatus.health;
    normal_.evidenceVolatile = normal_.storage != StorageHealth::DurableSd;
    if (normal_.storage != StorageHealth::DurableSd) {
      normal_.evidenceIncomplete = true;
      normal_.lastFailure = storageStatus.lastFailure;
    }
    return true;
  }

  RecorderResult start(const SessionStartRequest& request, SessionStatus& status) override {
    if (!normal_.controllerEpoch.valid()) {
      status = normal_;
      return RecorderResult::Invalid;
    }
    if (developmentActive()) {
      status = development_;
      return RecorderResult::AlreadyActive;
    }
    if (!request.controllerEpoch.valid() || request.controllerEpoch != normal_.controllerEpoch || !request.client.valid()) {
      status = development_;
      development_.lastFailure = FailureReason::InvalidRequest;
      return RecorderResult::Invalid;
    }
    development_ = SessionStatus{};
    development_.controllerEpoch = request.controllerEpoch;
    development_.session.value = ++nextDevelopmentSession_;
    development_.lifecycle = SessionLifecycle::Active;
    development_.storage = storage_.status().health;
    development_.evidenceVolatile = !storage_.status().durable;
    developmentClient_ = request.client;
    developmentStartedAt_ = lastNow_;
    developmentEverStarted_ = true;
    status = development_;
    return RecorderResult::Accepted;
  }

  RecorderResult startDevelopment(const SessionStartRequest& request, SessionStatus& status) {
    return start(request, status);
  }

  RecorderResult stop(SessionStatus& status) override {
    return stopDevelopment(status);
  }

  RecorderResult stopDevelopment(SessionStatus& status) override {
    if (!developmentActive()) { status = development_; return RecorderResult::NotActive; }
    development_.lifecycle = SessionLifecycle::Completed;
    status = development_;
    return RecorderResult::Accepted;
  }

  RecorderResult interrupt(SessionStatus& status) override {
    return interruptDevelopment(status);
  }

  RecorderResult interruptDevelopment(SessionStatus& status) {
    if (!developmentActive()) { status = development_; return RecorderResult::NotActive; }
    development_.lifecycle = SessionLifecycle::Interrupted;
    development_.evidenceIncomplete = true;
    status = development_;
    return RecorderResult::Accepted;
  }

  void tick(Time now) override {
    lastNow_ = now;
    if (developmentActive() && developmentStartedAt_ && now - developmentStartedAt_ >= DevelopmentLimitUs) {
      development_.lifecycle = SessionLifecycle::Completed;
      development_.lastFailure = FailureReason::None;
      developmentExpired_ = true;
    }
  }

  RecorderResult tryAppend(const RecordView& record, uint64_t& sequence) override {
    sequence = 0;
    if (!normal_.controllerEpoch.valid()) return RecorderResult::NotActive;
    // Reserve the sequence before touching storage.  A durable append can
    // return an error after bytes have reached the medium (for example a
    // short/ambiguous write or commit failure).  Advancing the recorder
    // watermark first prevents the next observation from reusing that
    // candidate and makes any resulting gap explicit in the incomplete/drop
    // accounting.
    const uint64_t candidate = ++nextSequence_;
    normal_.nextSequence = nextSequence_;
    if (developmentActive()) development_.nextSequence = nextSequence_;
    RecordView value = record;
    value.envelope.controllerEpoch = normal_.controllerEpoch;
    if (!value.envelope.session.valid()) value.envelope.session = normal_.session;
    StorageResult result = storage_.tryAppend(value, candidate);
    bool rolled = false;
    if (result == StorageResult::Full && storage_.evictOldest()) {
      rolled = true;
      result = storage_.tryAppend(value, candidate);
    }
    if (result != StorageResult::Accepted) {
      normal_.evidenceIncomplete = true;
      normal_.droppedRecords++;
      if (developmentActive()) development_.evidenceIncomplete = true;
      if (result == StorageResult::Full) {
        normal_.storage = StorageHealth::Degraded;
        normal_.lastFailure = FailureReason::StorageFull;
        if (developmentActive()) development_.storage = StorageHealth::Degraded;
        return RecorderResult::Dropped;
      }
      normal_.storage = StorageHealth::Fault;
      normal_.lastFailure = FailureReason::WriteFailure;
      if (developmentActive()) {
        development_.storage = StorageHealth::Fault;
        development_.lastFailure = FailureReason::WriteFailure;
        development_.droppedRecords++;
      }
      return RecorderResult::Fault;
    }
    normal_.bytesRecorded += value.payloadBytes;
    normal_.storage = storage_.status().health;
    if (rolled && developmentActive()) {
      development_.droppedRecords++;
      development_.evidenceIncomplete = true;
      development_.storage = StorageHealth::Degraded;
    }
    if (developmentActive()) {
      development_.nextSequence = nextSequence_;
      development_.bytesRecorded += value.payloadBytes;
    }
    sequence = nextSequence_;
    return RecorderResult::Accepted;
  }

  bool status(SessionStatus& status) const override {
    status = normal_;
    const StorageStatus current = storage_.status();
    status.storage = current.health;
    status.evidenceVolatile = !current.durable;
    if (current.health != StorageHealth::DurableSd || !current.durable) {
      status.evidenceIncomplete = true;
      if (status.lastFailure == FailureReason::None)
        status.lastFailure = current.lastFailure;
    }
    return normal_.controllerEpoch.valid();
  }

  bool developmentStatus(SessionStatus& status) const override {
    status = development_;
    return true;
  }

  bool storageStatus(StorageStatus& status) const override {
    status = storage_.status();
    return true;
  }

  size_t count() const override { return storage_.count(); }
  uint64_t revision() const override { return storage_.revision(); }
  bool read(size_t index, RecordEnvelope& envelope, uint8_t* payload,
            size_t capacity, size_t& length) const override {
    return storage_.read(index, envelope, payload, capacity, length);
  }

  bool developmentActive() const {
    return development_.lifecycle == SessionLifecycle::Starting ||
           development_.lifecycle == SessionLifecycle::Active ||
           development_.lifecycle == SessionLifecycle::Stopping;
  }
  bool developmentExpired() const { return developmentExpired_; }
  ClientId developmentClient() const { return developmentClient_; }

private:
  DiagnosticStorage& storage_;
  SessionStatus normal_{};
  SessionStatus development_{};
  ClientId normalClient_{};
  ClientId developmentClient_{};
  uint64_t nextDevelopmentSession_ = 0;
  uint64_t nextSequence_ = 0;
  Time developmentStartedAt_ = 0;
  Time lastNow_ = 1;
  bool developmentEverStarted_ = false;
  bool developmentExpired_ = false;
};

} // namespace diagnostic
} // namespace pp
