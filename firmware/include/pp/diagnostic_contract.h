#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core.h"

namespace pp {
namespace diagnostic {

// Phase 1 contract types only.  These types describe observations and do not
// participate in product or race authority.
enum class SessionLifecycle : uint8_t {
  Disabled = 0,
  Starting = 1,
  Active = 2,
  Stopping = 3,
  Completed = 4,
  Interrupted = 5,
};

enum class StorageHealth : uint8_t {
  DurableSd = 0,
  RamOnly = 1,
  Degraded = 2,
  Fault = 3,
};

enum class EventKind : uint8_t {
  UserAction = 0,
  BrowserRequest = 1,
  RequestAccepted = 2,
  RequestRejected = 3,
  BusRequest = 4,
  BusResult = 5,
  RaceDecision = 6,
  StateChange = 7,
  NoticeboardChanged = 8,
  SseSent = 9,
  BrowserRefresh = 10,
  BrowserRender = 11,
  PersistenceOperation = 12,
  ResourceHealth = 13,
  SessionStateChange = 14,
  StorageHealthChange = 15,
};

enum class Severity : uint8_t {
  Trace = 0,
  Info = 1,
  Warning = 2,
  Error = 3,
};

enum class RecorderResult : uint8_t {
  Accepted = 0,
  Dropped = 1,
  NotActive = 2,
  AlreadyActive = 3,
  Busy = 4,
  Fault = 5,
  Invalid = 6,
};

enum class StorageResult : uint8_t {
  Accepted = 0,
  Unavailable = 1,
  Full = 2,
  WriteFailed = 3,
  CommitFailed = 4,
  Corrupt = 5,
  UnsupportedVersion = 6,
  Busy = 7,
  Invalid = 8,
};

enum class FailureReason : uint8_t {
  None = 0,
  AlreadyActive = 1,
  StorageUnavailable = 2,
  StorageFull = 3,
  PayloadFailure = 4,
  CommitFailure = 5,
  CorruptRecord = 6,
  UnsupportedVersion = 7,
  RecorderFault = 8,
  InvalidRequest = 9,
  BackendUnavailable = 10,
  WriteFailure = 11,
};

struct ControllerEpoch {
  uint64_t value = 0;
  constexpr bool valid() const { return value != 0; }
  constexpr bool operator==(const ControllerEpoch& other) const { return value == other.value; }
  constexpr bool operator!=(const ControllerEpoch& other) const { return !(*this == other); }
};

struct SessionId {
  uint64_t value = 0;
  constexpr bool valid() const { return value != 0; }
};

struct ClientId {
  uint64_t value = 0;
  constexpr bool valid() const { return value != 0; }
};

// Correlation fields deliberately use the existing P&P request/event identity
// widths. Zero means that a particular relationship is not applicable.
struct Correlation {
  uint32_t request = 0;
  uint32_t event = 0;
  uint32_t parent = 0;
};

struct RecordEnvelope {
  static constexpr uint16_t CurrentFormatVersion = 1;

  uint16_t formatVersion = CurrentFormatVersion;
  EventKind kind = EventKind::StateChange;
  Severity severity = Severity::Info;
  ControllerEpoch controllerEpoch{};
  SessionId session{};
  ClientId client{};
  uint64_t sequence = 0;
  Time timestamp = 0;
  Correlation correlation{};
  uint32_t payloadType = 0;
};

// The payload is borrowed for the duration of tryAppend(). Backends own any
// copy they retain; callers never transfer ownership through this interface.
struct RecordView {
  RecordEnvelope envelope{};
  const uint8_t* payload = nullptr;
  size_t payloadBytes = 0;
};

struct SessionStartRequest {
  ControllerEpoch controllerEpoch{};
  ClientId client{};
};

struct SessionStatus {
  ControllerEpoch controllerEpoch{};
  SessionId session{};
  SessionLifecycle lifecycle = SessionLifecycle::Disabled;
  StorageHealth storage = StorageHealth::Fault;
  FailureReason lastFailure = FailureReason::None;
  uint64_t nextSequence = 0;
  uint64_t droppedRecords = 0;
  bool evidenceVolatile = false;
  bool evidenceIncomplete = false;
};

struct StorageStatus {
  StorageHealth health = StorageHealth::Fault;
  FailureReason lastFailure = FailureReason::None;
  bool durable = false;
};

// A recorder implementation must make tryAppend bounded and non-blocking from
// the caller's perspective. It may drop a diagnostic record, but must report
// that outcome and must never affect product timing or authority.
class DiagnosticRecorder {
public:
  virtual ~DiagnosticRecorder() = default;

  // If a session is already active, return AlreadyActive and populate status
  // with the existing session identity. This never joins or replaces it.
  virtual RecorderResult start(const SessionStartRequest& request,
                               SessionStatus& status) = 0;
  virtual RecorderResult stop(SessionStatus& status) = 0;
  virtual RecorderResult interrupt(SessionStatus& status) = 0;
  virtual RecorderResult tryAppend(const RecordView& record, uint64_t& sequence) = 0;
  virtual bool status(SessionStatus& status) const = 0;
  virtual size_t count() const = 0;
  virtual bool read(size_t index, RecordEnvelope& envelope, uint8_t* payload,
                    size_t capacity, size_t& length) const = 0;
};

// Storage is a backend contract only. Phase 1 defines no SD, file, NVS, RAM,
// or failure-injection implementation.
class DiagnosticStorage {
public:
  virtual ~DiagnosticStorage() = default;

  virtual StorageResult begin(ControllerEpoch controllerEpoch) = 0;
  virtual StorageResult tryAppend(const RecordView& record, uint64_t sequence) = 0;
  virtual size_t count() const = 0;
  virtual bool read(size_t index, RecordEnvelope& envelope, uint8_t* payload,
                    size_t capacity, size_t& length) const = 0;
  virtual StorageStatus status() const = 0;
  virtual StorageResult recover() = 0;
  virtual StorageResult close() = 0;
};

} // namespace diagnostic
} // namespace pp

