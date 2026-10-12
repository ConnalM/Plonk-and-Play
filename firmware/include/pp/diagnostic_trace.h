#pragma once

#include "core.h"
#include "diagnostic_contract.h"

namespace pp {
namespace diagnostic {

// Observation-only trace spine. Browser/HTTP observations are joined to the
// existing P&P request path by product correlation and a separate traceId;
// neither is used to authorise or alter product requests.
class ControllerOperationTrace final {
public:
  explicit ControllerOperationTrace(DiagnosticRecorder& recorder)
      : recorder_(recorder) { observerClient_.value = 1; }
  explicit ControllerOperationTrace(DiagnosticRecorder& recorder,
                                    ClientId observerClient)
      : recorder_(recorder), observerClient_(observerClient) {}

  void observeBrowserAction(OperationName operation, uint32_t visibleCorrelation,
                            ClientId client, uint64_t traceId,
                            Time timestamp = 0) {
    Correlation correlation{};
    correlation.request = visibleCorrelation;
    append(EventKind::UserAction, operation, correlation, 0, 0, timestamp,
           client, traceId);
  }

  // Called after the normal HTTP/interface path has accepted a product request
  // for bus publication. It records the Browser correlation and joins it to
  // the internal bus correlation without changing either.
  void observeInterfaceRequest(OperationName operation, uint32_t visibleCorrelation,
                               uint32_t internalCorrelation, ClientId client,
                               uint64_t traceId, Time timestamp = 0) {
    Correlation correlation{};
    correlation.request = visibleCorrelation;
    correlation.parent = internalCorrelation;
    append(EventKind::BrowserRequest, operation, correlation, 0, 0, timestamp,
           client, traceId);
    if (!internalCorrelation) return;
    Pending* pending = allocate(internalCorrelation);
    if (!pending) return;
    pending->operation = operation;
    pending->visibleCorrelation = visibleCorrelation;
    pending->client = client;
    pending->traceId = traceId;
    pending->timestamp = timestamp;
  }

  void observe(const Message& message, uint8_t authoritativeLifecycle,
               Time timestamp = 0) {
    switch (message.type) {
      case Type::StartRequest:
      case Type::SetupRequest:
      case Type::SessionOperationRequest: {
        const OperationName operation = operationFor(message);
        Pending* pending = find(message.correlation);
        if (!pending) pending = allocate(message.correlation);
        if (!pending) { ++droppedObservations_; return; }
        pending->operation = operation;
        pending->awaitsNotice = operation == OperationName::Start;
        if (!pending->client.valid()) pending->client = observerClient_;
        Correlation correlation{};
        correlation.request = message.correlation;
        correlation.parent = pending->visibleCorrelation;
        correlation.event = message.eventId;
        append(EventKind::BusRequest, operation, correlation,
               authoritativeLifecycle, 0, timestamp, pending->client,
               pending->traceId);
        break;
      }
      case Type::NoticeboardChanged: {
        Pending* pending = latestNoticePending();
        if (!pending) return;
        pending->noticeSeen = true;
        pending->lifecycle = authoritativeLifecycle;
        pending->eventId = message.eventId;
        pending->timestamp = timestamp;
        if (pending->resultSeen) flushNotice(*pending);
        break;
      }
      case Type::RequestResult: {
        Pending* pending = find(message.correlation);
        if (!pending) return;
        Correlation correlation{};
        correlation.request = message.correlation;
        correlation.parent = pending->visibleCorrelation;
        correlation.event = message.eventId;
        append(message.requestResult == RequestResult::Accepted
                   ? EventKind::RequestAccepted
                   : EventKind::RequestRejected,
               pending->operation,
               correlation,
               authoritativeLifecycle,
               static_cast<uint32_t>(message.rejection) +
                   (message.requestResult == RequestResult::Accepted ? 0u : 0x100u),
               timestamp, pending->client, pending->traceId);
        if (message.requestResult == RequestResult::Accepted && pending->awaitsNotice) {
          pending->resultSeen = true;
          if (pending->noticeSeen) flushNotice(*pending);
        } else {
          *pending = Pending{};
        }
        break;
      }
      case Type::Count:
      default:
        break;
    }
  }

  // Browser presentation observations are evidence only.  The generic
  // Noticeboard contract remains unchanged; revision/event values are carried
  // in diagnostic correlation fields where the Browser has them.
  void observeBrowserNotification(ClientId client, uint64_t traceId,
                                  uint32_t visibleCorrelation,
                                  uint32_t revision, uint32_t eventId,
                                  uint32_t notificationType,
                                  OperationName operation = OperationName::Unknown,
                                  Time timestamp = 0) {
    Correlation correlation{};
    correlation.request = visibleCorrelation;
    correlation.event = revision;
    correlation.parent = eventId;
    append(EventKind::BrowserNotification, operation, correlation,
           0, notificationType, timestamp, client, traceId);
  }

  void observeBrowserState(EventKind kind, ClientId client, uint64_t traceId,
                          uint32_t visibleCorrelation, uint32_t revision,
                          uint8_t lifecycle, uint8_t sessionMode,
                          uint32_t detail = 0,
                          OperationName operation = OperationName::Unknown,
                          Time timestamp = 0) {
    Correlation correlation{};
    correlation.request = visibleCorrelation;
    correlation.event = revision;
    append(kind, operation, correlation, lifecycle, detail,
           timestamp, client, traceId, sessionMode);
  }

  uint64_t droppedObservations() const { return droppedObservations_; }

private:
  static constexpr size_t PendingCapacity = 8;
  struct Pending {
    bool active = false;
    bool resultSeen = false;
    bool noticeSeen = false;
    bool awaitsNotice = false;
    uint32_t internalCorrelation = 0;
    uint32_t visibleCorrelation = 0;
    uint32_t eventId = 0;
    uint8_t lifecycle = 0;
    Time timestamp = 0;
    ClientId client{};
    uint64_t traceId = 0;
    OperationName operation = OperationName::Unknown;
  };

  static OperationName operationFor(const Message& message) {
    if (message.type == Type::StartRequest) return OperationName::Start;
    if (message.type == Type::SetupRequest) return OperationName::Setup;
    switch (message.operation) {
      case SessionOperation::Pause: return OperationName::Pause;
      case SessionOperation::Resume: return OperationName::Resume;
      case SessionOperation::RestartRace: return OperationName::RestartRace;
      case SessionOperation::EndRace: return OperationName::EndRace;
      case SessionOperation::RaceAgain: return OperationName::RaceAgain;
      case SessionOperation::EndSession: return OperationName::EndSession;
      case SessionOperation::SkipFinishDisplay: return OperationName::SkipFinishDisplay;
      default: return OperationName::Unknown;
    }
  }

  Pending* find(uint32_t internalCorrelation) {
    for (auto& pending : pending_)
      if (pending.active && pending.internalCorrelation == internalCorrelation) return &pending;
    return nullptr;
  }
  Pending* allocate(uint32_t internalCorrelation) {
    if (!internalCorrelation) return nullptr;
    for (auto& pending : pending_) if (!pending.active) {
      pending = Pending{};
      pending.active = true;
      pending.internalCorrelation = internalCorrelation;
      return &pending;
    }
    ++droppedObservations_;
    return nullptr;
  }
  Pending* latestNoticePending() {
    Pending* selected = nullptr;
    for (auto& pending : pending_)
      if (pending.active && pending.awaitsNotice &&
          (!selected || pending.internalCorrelation > selected->internalCorrelation)) selected = &pending;
    return selected;
  }
  void flushNotice(Pending& pending) {
    if (!pending.resultSeen || !pending.noticeSeen) return;
    Correlation correlation{};
    correlation.request = pending.internalCorrelation;
    correlation.parent = pending.visibleCorrelation;
    correlation.event = pending.eventId;
    append(EventKind::StateChange, pending.operation, correlation,
           pending.lifecycle, pending.lifecycle, pending.timestamp,
           pending.client, pending.traceId);
    append(EventKind::NoticeboardChanged, pending.operation, correlation,
           pending.lifecycle, 0, pending.timestamp,
           pending.client, pending.traceId);
    pending = Pending{};
  }

  void append(EventKind kind, OperationName operation, Correlation correlation,
              uint8_t lifecycle, uint32_t payloadType, Time timestamp,
              ClientId client, uint64_t traceId, uint8_t sessionMode = 0xFF) {
    SessionStatus status{};
    if (!recorder_.status(status)) { ++droppedObservations_; return; }
    const uint8_t payload[2] = {
      lifecycle, sessionMode == 0xFF ? static_cast<uint8_t>(payloadType) : sessionMode};
    RecordView record{};
    record.envelope.controllerEpoch = status.controllerEpoch;
    record.envelope.session = status.session;
    record.envelope.client = client.valid() ? client : observerClient_;
    record.envelope.timestamp = timestamp;
    record.envelope.kind = kind;
    record.envelope.severity = kind == EventKind::RequestRejected
                                   ? Severity::Warning : Severity::Info;
    record.envelope.correlation = correlation;
    record.envelope.traceId = traceId;
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
  Pending pending_[PendingCapacity]{};
  uint64_t droppedObservations_ = 0;
};

} // namespace diagnostic
} // namespace pp
