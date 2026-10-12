#include "pp/diagnostic_contract.h"

using namespace pp::diagnostic;

static_assert(static_cast<unsigned>(SessionLifecycle::Disabled) == 0, "lifecycle contract");
static_assert(static_cast<unsigned>(SessionLifecycle::Active) == 2, "lifecycle contract");
static_assert(static_cast<unsigned>(SessionLifecycle::Interrupted) == 5, "lifecycle contract");
static_assert(static_cast<unsigned>(StorageHealth::RamOnly) == 1, "storage contract");
static_assert(static_cast<unsigned>(RecorderResult::AlreadyActive) == 3, "already-active contract");
static_assert(RecordEnvelope::CurrentFormatVersion == 2, "record version contract");

class ContractRecorder final : public DiagnosticRecorder {
public:
  RecorderResult start(const SessionStartRequest& request, SessionStatus& out) override {
    if (status_.lifecycle == SessionLifecycle::Active) { out = status_; return RecorderResult::AlreadyActive; }
    status_.controllerEpoch = request.controllerEpoch;
    status_.session.value = 1;
    status_.lifecycle = SessionLifecycle::Active;
    status_.storage = StorageHealth::RamOnly;
    status_.evidenceVolatile = true;
    out = status_;
    return RecorderResult::Accepted;
  }
  RecorderResult stop(SessionStatus& out) override {
    status_.lifecycle = SessionLifecycle::Completed;
    out = status_;
    return RecorderResult::Accepted;
  }
  RecorderResult interrupt(SessionStatus& out) override {
    status_.lifecycle = SessionLifecycle::Interrupted;
    out = status_;
    return RecorderResult::Accepted;
  }
  RecorderResult tryAppend(const RecordView&, uint64_t& sequence) override {
    sequence = ++status_.nextSequence;
    return status_.lifecycle == SessionLifecycle::Active ? RecorderResult::Accepted : RecorderResult::NotActive;
  }
  bool status(SessionStatus& out) const override { out = status_; return true; }
  size_t count() const override { return 0; }
  bool read(size_t, RecordEnvelope&, uint8_t*, size_t, size_t& length) const override { length = 0; return false; }
private:
  SessionStatus status_{};
};

int main() {
  ContractRecorder recorder;
  SessionStatus status{};
  SessionStartRequest request{};
  request.controllerEpoch.value = 1;
  request.client.value = 1;
  if (recorder.start(request, status) != RecorderResult::Accepted) return 1;
  SessionStatus existing{};
  if (recorder.start(request, existing) != RecorderResult::AlreadyActive) return 2;
  if (!existing.session.valid() || existing.lifecycle != SessionLifecycle::Active) return 3;
  if (status.storage != StorageHealth::RamOnly || !status.evidenceVolatile) return 4;
  return 0;
}

