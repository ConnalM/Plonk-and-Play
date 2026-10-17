#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "diagnostic_contract.h"

namespace pp {
namespace diagnostic {

// The support download is deliberately encoded independently of HTTP.  The
// HTTP route supplies a bounded chunk sink, while host acceptance uses the
// same encoder with a string sink.  Neither sink can change product state.
class DiagnosticExportSink {
public:
  virtual ~DiagnosticExportSink() = default;
  virtual bool write(const char* data, size_t length) = 0;
};

struct DiagnosticExportMetadata {
  const char* firmwareBuild = "unknown";
  const char* backend = "unknown";
  const char* capability = "UNAVAILABLE";
  uint8_t storageHealth = static_cast<uint8_t>(StorageHealth::Fault);
  ControllerEpoch controllerEpoch{};
  SessionId normalSession{};
  SessionId developmentSession{};
  SessionLifecycle developmentLifecycle = SessionLifecycle::Disabled;
  bool evidenceVolatile = false;
  bool evidenceIncomplete = false;
};

struct DiagnosticExportResult {
  size_t snapshotRecords = 0;
  size_t exportedRecords = 0;
  uint64_t snapshotRevision = 0;
  bool complete = false;
  bool readFailed = false;
  bool changedDuringExport = false;
  bool duplicateSequence = false;
  bool writeFailed = false;
};

class DiagnosticExport final {
public:
  static constexpr const char* Schema = "pp-diagnostics-v1";
  static constexpr size_t MaxPayloadBytes = 256;
  static constexpr size_t LineBytes = 1536;

  static const char* capability(const StorageStatus& storage, size_t retained) {
    if (storage.health == StorageHealth::DurableSd)
      return "AVAILABLE";
    if (retained != 0 || storage.health == StorageHealth::RamOnly ||
        storage.health == StorageHealth::Degraded)
      return "LIMITED";
    return "UNAVAILABLE";
  }

  static bool write(DiagnosticRecorder& recorder,
                    const DiagnosticExportMetadata& metadata,
                    uint64_t droppedRecords,
                    DiagnosticExportSink& sink,
                    DiagnosticExportResult& result) {
    result = DiagnosticExportResult{};
    result.snapshotRecords = recorder.count();
    result.snapshotRevision = recorder.revision();
    char line[LineBytes]{};
    const int header = snprintf(
        line, sizeof(line),
        "{\"type\":\"header\",\"schema\":\"%s\",\"product\":\"Plonk-and-Play\","
        "\"firmwareBuild\":\"%s\",\"backend\":\"%s\",\"storageHealth\":%u,"
        "\"controllerEpoch\":%llu,\"normalSession\":%llu,\"developmentSession\":%llu,"
        "\"developmentLifecycle\":%u,\"exportCapability\":\"%s\","
        "\"evidenceVolatile\":%s,\"evidenceIncomplete\":%s,"
        "\"snapshotRecords\":%u,\"snapshotRevision\":%llu,"
        "\"completeness\":\"reported-in-summary\"}\n",
        Schema, metadata.firmwareBuild, metadata.backend,
        static_cast<unsigned>(metadata.storageHealth),
        static_cast<unsigned long long>(metadata.controllerEpoch.value),
        static_cast<unsigned long long>(metadata.normalSession.value),
        static_cast<unsigned long long>(metadata.developmentSession.value),
        static_cast<unsigned>(metadata.developmentLifecycle), metadata.capability,
        metadata.evidenceVolatile ? "true" : "false",
        metadata.evidenceIncomplete ? "true" : "false",
        static_cast<unsigned>(result.snapshotRecords),
        static_cast<unsigned long long>(result.snapshotRevision));
    if (header <= 0 || static_cast<size_t>(header) >= sizeof(line) ||
        !sink.write(line, static_cast<size_t>(header))) {
      result.writeFailed = true;
      return false;
    }

    uint8_t payload[MaxPayloadBytes]{};
    static const char hex[] = "0123456789ABCDEF";
    uint64_t previousSequence = 0;
    bool havePreviousSequence = false;
    for (size_t index = 0; index < result.snapshotRecords; ++index) {
      if (recorder.revision() != result.snapshotRevision) {
        result.changedDuringExport = true;
        break;
      }
      RecordEnvelope envelope{};
      size_t payloadBytes = 0;
      if (!recorder.read(index, envelope, payload, sizeof(payload), payloadBytes)) {
        result.readFailed = true;
        break;
      }
      // Records are returned in chronological storage order. A duplicate (or
      // regression) therefore violates the durable monotonic sequence
      // contract and can be detected from the previous record alone. This
      // keeps export O(n) instead of rereading the SD file for every record.
      if (havePreviousSequence && envelope.sequence <= previousSequence)
        result.duplicateSequence = true;
      previousSequence = envelope.sequence;
      havePreviousSequence = true;
      int prefix = snprintf(
          line, sizeof(line),
          "{\"type\":\"record\",\"sequence\":%llu,\"timestamp\":%llu,"
          "\"kind\":%u,\"severity\":%u,\"controllerEpoch\":%llu,"
          "\"session\":%llu,\"client\":%llu,\"traceId\":%llu,"
          "\"request\":%u,\"event\":%u,\"parent\":%u,\"payloadType\":%u,"
          "\"operation\":%u,\"payloadHex\":\"",
          static_cast<unsigned long long>(envelope.sequence),
          static_cast<unsigned long long>(envelope.timestamp),
          static_cast<unsigned>(envelope.kind), static_cast<unsigned>(envelope.severity),
          static_cast<unsigned long long>(envelope.controllerEpoch.value),
          static_cast<unsigned long long>(envelope.session.value),
          static_cast<unsigned long long>(envelope.client.value),
          static_cast<unsigned long long>(envelope.traceId),
          static_cast<unsigned>(envelope.correlation.request),
          static_cast<unsigned>(envelope.correlation.event),
          static_cast<unsigned>(envelope.correlation.parent),
          static_cast<unsigned>(envelope.payloadType),
          static_cast<unsigned>(envelope.operation));
      if (prefix <= 0 || static_cast<size_t>(prefix) >= sizeof(line) ||
          payloadBytes > (sizeof(line) - static_cast<size_t>(prefix) - 4U) / 2U) {
        result.readFailed = true;
        break;
      }
      size_t used = static_cast<size_t>(prefix);
      for (size_t p = 0; p < payloadBytes; ++p) {
        line[used++] = hex[payload[p] >> 4];
        line[used++] = hex[payload[p] & 0x0F];
      }
      line[used++] = '"';
      line[used++] = '}';
      line[used++] = '\n';
      if (!sink.write(line, used)) {
        result.writeFailed = true;
        return false;
      }
      ++result.exportedRecords;
      if (recorder.revision() != result.snapshotRevision) {
        result.changedDuringExport = true;
        break;
      }
    }

    result.complete = !result.readFailed && !result.changedDuringExport &&
                      !result.duplicateSequence &&
                      result.exportedRecords == result.snapshotRecords;
    const bool incomplete = metadata.evidenceIncomplete || !result.complete;
    const int summary = snprintf(
        line, sizeof(line),
        "{\"type\":\"summary\",\"snapshotRecords\":%u,\"records\":%u,"
        "\"complete\":%s,\"readFailed\":%s,\"changedDuringExport\":%s,"
        "\"duplicateSequence\":%s,"
        "\"dropped\":%llu,\"evidenceVolatile\":%s,\"evidenceIncomplete\":%s,"
        "\"exportCapability\":\"%s\"}\n",
        static_cast<unsigned>(result.snapshotRecords),
        static_cast<unsigned>(result.exportedRecords), result.complete ? "true" : "false",
        result.readFailed ? "true" : "false",
        result.changedDuringExport ? "true" : "false",
        result.duplicateSequence ? "true" : "false",
        static_cast<unsigned long long>(droppedRecords),
        metadata.evidenceVolatile ? "true" : "false",
        incomplete ? "true" : "false",
        metadata.capability);
    if (summary <= 0 || static_cast<size_t>(summary) >= sizeof(line) ||
        !sink.write(line, static_cast<size_t>(summary))) {
      result.writeFailed = true;
      return false;
    }
    return true;
  }

};

} // namespace diagnostic
} // namespace pp
