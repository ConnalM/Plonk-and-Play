#pragma once

#include "core.h"
#include "diagnostic_contract.h"

namespace pp {
namespace diagnostic {

// ControllerOperationTrace is an observation-only spine for one named
// operation.  It buffers a NoticeboardChanged observation until the matching
// RequestResult is seen so a rejected request can never acquire a false state
// transition in the diagnostic evidence.
class ControllerOperationTrace final {
public:
  explicit ControllerOperationTrace(DiagnosticRecorder& recorder)
      : recorder_(recorder) { observerClient_.value = 1; }
  explicit ControllerOperationTrace(DiagnosticRecorder& recorder,
                                    ClientId observerClient)
      : recorder_(recorder), observerClient_(observerClient) {}

  void observe(const Message& message, uint8_t authoritativeLifecycle,
               Time timestamp = 0) {
    switch (message.type) {
      case Type::StartRequest:
      {
        pending_ = Pending{};
        pending_.active = message.correlation != 0;
        pending_.correlation = message.correlation;
        Correlation correlation{};
        correlation.request = message.correlation;
        correlation.event = message.eventId;
        append(EventKind::BusRequest, OperationName::Start, correlation,
               authoritativeLifecycle, 0, timestamp);
        break;
      }
      case Type::NoticeboardChanged:
        if (pending_.active) {
          pending_.noticeSeen = true;
          pending_.lifecycle = authoritativeLifecycle;
          pending_.eventId = message.eventId;
          pending_.timestamp = timestamp;
        }
        break;
      case Type::RequestResult:
      {
        if (!pending_.active || message.correlation != pending_.correlation) return;
        Correlation correlation{};
        correlation.request = message.correlation;
        correlation.event = message.eventId;
        append(message.requestResult == RequestResult::Accepted
                   ? EventKind::RequestAccepted
                   : EventKind::RequestRejected,
               OperationName::Start,
               correlation,
               authoritativeLifecycle, static_cast<uint32_t>(message.rejection) +
                   (message.requestResult == RequestResult::Accepted ? 0u : 0x100u), timestamp);
        if (message.requestResult == RequestResult::Accepted) {
          pending_.resultSeen = true;
          flushNotice();
        } else {
          pending_ = Pending{};
        }
        break;
      }
      case Type::Count:
      default:
        break;
    }
  }

  uint64_t droppedObservations() const { return droppedObservations_; }

private:
  struct Pending {
    bool active = false;
    bool resultSeen = false;
    bool noticeSeen = false;
    uint32_t correlation = 0;
    uint32_t eventId = 0;
    uint8_t lifecycle = 0;
    Time timestamp = 0;
  };

  void flushNotice() {
    if (!pending_.resultSeen || !pending_.noticeSeen) return;
    Correlation correlation{};
    correlation.request = pending_.correlation;
    correlation.event = pending_.eventId;
    append(EventKind::StateChange, OperationName::Start, correlation,
           pending_.lifecycle, pending_.lifecycle, pending_.timestamp);
    append(EventKind::NoticeboardChanged, OperationName::Start, correlation,
           pending_.lifecycle, 0, pending_.timestamp);
    pending_ = Pending{};
  }

  void append(EventKind kind, OperationName operation, Correlation correlation,
              uint8_t lifecycle, uint32_t payloadType, Time timestamp) {
    SessionStatus status{};
    if (!recorder_.status(status)) {
      ++droppedObservations_;
      return;
    }
    const uint8_t payload[2] = {lifecycle, static_cast<uint8_t>(payloadType)};
    RecordView record{};
    record.envelope.controllerEpoch = status.controllerEpoch;
    record.envelope.session = status.session;
    record.envelope.client = observerClient_;
    record.envelope.timestamp = timestamp;
    record.envelope.kind = kind;
    record.envelope.severity = kind == EventKind::RequestRejected
                                   ? Severity::Warning
                                   : Severity::Info;
    record.envelope.correlation = correlation;
    record.envelope.payloadType = payloadType;
    record.envelope.operation = operation;
    record.payload = payload;
    record.payloadBytes = sizeof(payload);
    uint64_t sequence = 0;
    if (recorder_.tryAppend(record, sequence) != RecorderResult::Accepted)
      ++droppedObservations_;
  }

  DiagnosticRecorder& recorder_;
  ClientId observerClient_;
  Pending pending_{};
  uint64_t droppedObservations_ = 0;
};

} // namespace diagnostic
} // namespace pp
