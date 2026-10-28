#pragma once

// WP4A production diagnostic backend for the ESP32-S3 SD/FATFS path. This
// stores observation records only and never participates in race authority.

#include <Arduino.h>
#include <FS.h>
#include <string.h>

#include "diagnostic_contract.h"
#include "physical_sd_owner.h"

namespace pp {
namespace diagnostic {

class Esp32SpiSdDiagnosticStorage final : public DiagnosticStorage {
public:
  static constexpr int CsPin = PhysicalSdOwner::CsPin;
  static constexpr int MosiPin = PhysicalSdOwner::MosiPin;
  static constexpr int SckPin = PhysicalSdOwner::SckPin;
  static constexpr int MisoPin = PhysicalSdOwner::MisoPin;
  static constexpr uint32_t SpiFrequency = PhysicalSdOwner::SpiFrequency;

  // Provisional WP4 candidate bounds, pending real-card measurements.
  static constexpr size_t Capacity = 256;
  static constexpr size_t MaxPayloadBytes = 256;
  static constexpr size_t HeaderBytes = 16;
  static constexpr size_t EnvelopeBytes = 72;
  static constexpr size_t RecordOverheadBytes = 4 + EnvelopeBytes + 4;
  static constexpr size_t MaxFileBytes =
      HeaderBytes + Capacity * (RecordOverheadBytes + MaxPayloadBytes);

  explicit Esp32SpiSdDiagnosticStorage(PhysicalSdOwner& owner)
      : owner_(owner) {}

  StorageResult begin(ControllerEpoch epoch) override {
    if (!epoch.valid())
      return fail(StorageResult::Invalid, FailureReason::InvalidRequest);
    epoch_ = epoch;
    auto lease = owner_.lock();
    if (!lease || !owner_.available())
      return fail(StorageResult::Unavailable, FailureReason::BackendUnavailable);
    closeReadCursor();
    mounted_ = true;
    owner_.mkdir(RootPath);
    if (owner_.exists(FilePath)) {
      if (!scanExisting()) {
        char corruptPath[64]{};
        snprintf(corruptPath, sizeof(corruptPath), "%s.corrupt-%llu", RootPath,
                 static_cast<unsigned long long>(epoch.value));
        if (!owner_.rename(FilePath, corruptPath))
          return fail(StorageResult::Corrupt, FailureReason::CorruptRecord);
        health_ = StorageHealth::Degraded;
        lastFailure_ = FailureReason::CorruptRecord;
        if (!createFile())
          return fail(StorageResult::WriteFailed, FailureReason::WriteFailure);
      }
    } else if (!createFile()) {
      return fail(StorageResult::WriteFailed, FailureReason::WriteFailure);
    }
    open_ = true;
    if (health_ != StorageHealth::Degraded) {
      health_ = StorageHealth::DurableSd;
      lastFailure_ = FailureReason::None;
    }
    return StorageResult::Accepted;
  }

  StorageResult tryAppend(const RecordView& record, uint64_t sequence) override {
    auto lease = owner_.lock();
    if (!lease || !open_)
      return fail(StorageResult::Unavailable, FailureReason::BackendUnavailable);
    if (record.payloadBytes > MaxPayloadBytes ||
        (record.payloadBytes != 0 && !record.payload))
      return fail(StorageResult::Invalid, FailureReason::PayloadFailure);
    const size_t body = EnvelopeBytes + record.payloadBytes;
    const size_t frame = 4 + body + 4;
    if (recordCount_ >= Capacity || fileBytes_ + frame > MaxFileBytes)
      return fail(StorageResult::Full, FailureReason::StorageFull);

    uint8_t bytes[RecordOverheadBytes + MaxPayloadBytes]{};
    size_t pos = 0;
    put32(bytes, pos, static_cast<uint32_t>(body));
    encodeEnvelope(record.envelope, sequence, bytes, pos);
    if (record.payloadBytes) {
      memcpy(bytes + pos, record.payload, record.payloadBytes);
      pos += record.payloadBytes;
    }
    put32(bytes, pos, checksum(bytes, pos));
    // Do not retain an append handle while the export path opens a read
    // handle.  Arduino SD/FATFS on the S3 does not reliably support those two
    // handles concurrently: the count can advance while the first read
    // fails.  A short-lived append handle keeps writes and reads independent.
    File output = owner_.open(FilePath, FILE_APPEND);
    if (!output) {
      reportPhysicalIoFailure(lease);
      return fail(StorageResult::WriteFailed, FailureReason::WriteFailure);
    }
    const size_t written = output.write(bytes, pos);
    output.flush();
    output.close();
    if (written != pos) {
      reportPhysicalIoFailure(lease);
      return fail(StorageResult::WriteFailed, FailureReason::WriteFailure);
    }
    ++recordCount_;
    fileBytes_ += pos;
    if (sequence > lastSequence_)
      lastSequence_ = sequence;
    ++revision_;
    return StorageResult::Accepted;
  }

  size_t count() const override { return recordCount_; }

  bool read(size_t wanted, RecordEnvelope& envelope, uint8_t* payload,
            size_t capacity, size_t& length) const override {
    auto lease = owner_.lock();
    if (!lease) { length = 0; return false; }
    length = 0;
    if (!mounted_ || wanted >= recordCount_)
      return false;
    // Keep no filesystem handle across calls. The owner lease therefore
    // covers the complete bounded read and append/export operations cannot
    // overlap on Arduino SD/FATFS.
    File input = owner_.open(FilePath, FILE_READ);
    if (!input || !input.seek(HeaderBytes)) {
      if (input) input.close();
      reportPhysicalIoFailure(lease);
      return false;
    }
    uint8_t frame[RecordOverheadBytes + MaxPayloadBytes]{};
    for (size_t index = 0; index <= wanted; ++index) {
      uint32_t body = 0;
      if (!readFrame(input, frame, body)) {
        input.close();
        return false;
      }
      if (index != wanted)
        continue;
      const size_t actual = body - EnvelopeBytes;
      if (actual > capacity || (actual && !payload)) {
        input.close();
        return false;
      }
      size_t pos = 4;
      decodeEnvelope(frame, pos, envelope);
      if (actual)
        memcpy(payload, frame + pos, actual);
      length = actual;
      input.close();
      return true;
    }
    input.close();
    return false;
  }

  StorageStatus status() const override {
    StorageStatus value{};
    value.health = owner_.available() ? health_ : StorageHealth::Fault;
    value.backend = BackendKind::ProductionSd;
    value.lastFailure = owner_.available() ? lastFailure_ : FailureReason::BackendUnavailable;
    value.durable = owner_.available() && mounted_ && health_ == StorageHealth::DurableSd;
    return value;
  }

  StorageResult recover() override {
    auto lease = owner_.lock();
    if (!lease || !owner_.available())
      return fail(StorageResult::Unavailable, FailureReason::BackendUnavailable);
    mounted_ = true;
    closeReadCursor();
    if (!scanExisting()) {
      health_ = StorageHealth::Degraded;
      lastFailure_ = FailureReason::CorruptRecord;
      return StorageResult::Corrupt;
    }
    health_ = StorageHealth::DurableSd;
    lastFailure_ = FailureReason::None;
    return StorageResult::Accepted;
  }

  StorageResult close() override {
    closeReadCursor();
    if (file_)
      file_.close();
    open_ = false;
    return StorageResult::Accepted;
  }

  // Rolling is performed only at the bounded-capacity boundary, never on an
  // ordinary append. A failed compaction is reported as a diagnostic fault;
  // it cannot affect product state or cause a retry loop.
  bool evictOldest() override {
    auto lease = owner_.lock();
    if (!lease) return failCompaction();
    if (!open_ || recordCount_ == 0) return false;
    closeReadCursor();
    file_.close();
    open_ = false;
    const char* temporaryPath = "/pp-diagnostics/records.roll.tmp";
    owner_.remove(temporaryPath);
    File input = owner_.open(FilePath, FILE_READ);
    File output = owner_.open(temporaryPath, FILE_WRITE);
    if (!input || !output) {
      if (input) input.close();
      if (output) output.close();
      return failCompaction();
    }
    uint8_t header[HeaderBytes]{};
    uint8_t frame[RecordOverheadBytes + MaxPayloadBytes]{};
    uint32_t body = 0;
    bool ok = input.read(header, sizeof(header)) == sizeof(header) &&
              output.write(header, sizeof(header)) == sizeof(header) &&
              readFrame(input, frame, body);
    while (ok && input.available()) {
      ok = readFrame(input, frame, body) &&
           output.write(frame, 4 + body + 4) == 4 + body + 4;
    }
    input.close();
    output.flush();
    output.close();
    if (!ok || !owner_.remove(FilePath) || !owner_.rename(temporaryPath, FilePath))
      return failCompaction();
    recordCount_--;
    File refreshed = owner_.open(FilePath, FILE_READ);
    fileBytes_ = refreshed ? refreshed.size() : 0;
    if (refreshed) refreshed.close();
    open_ = true;
    ++revision_;
    return true;
  }
  uint64_t lastSequence() const override { return lastSequence_; }
  uint64_t revision() const override { return revision_; }

private:
  static constexpr const char* RootPath = "/pp-diagnostics";
  static constexpr const char* FilePath = "/pp-diagnostics/records.bin";
  static constexpr uint32_t HeaderMagic = 0x44494147U; // DIAG
  static constexpr uint16_t FormatVersion = 1;

  StorageResult fail(StorageResult result, FailureReason reason) {
    if (result != StorageResult::Full)
      health_ = StorageHealth::Fault;
    else
      health_ = StorageHealth::Degraded;
    lastFailure_ = reason;
    return result;
  }

  bool failCompaction() {
    health_ = StorageHealth::Degraded;
    lastFailure_ = FailureReason::StorageFull;
    return false;
  }

  bool createFile() {
    closeReadCursor();
    owner_.remove(FilePath);
    File output = owner_.open(FilePath, FILE_WRITE);
    if (!output)
      return false;
    uint8_t header[HeaderBytes]{};
    size_t pos = 0;
    put32(header, pos, HeaderMagic);
    put16(header, pos, FormatVersion);
    put16(header, pos, 0);
    put64(header, pos, epoch_.value);
    const bool ok = output.write(header, sizeof(header)) == sizeof(header);
    output.flush();
    output.close();
    recordCount_ = 0;
    fileBytes_ = HeaderBytes;
    lastSequence_ = 0;
    return ok;
  }

  bool scanExisting() const {
    closeReadCursor();
    File input = owner_.open(FilePath, FILE_READ);
    if (!input)
      return false;
    uint8_t header[HeaderBytes]{};
    if (input.read(header, sizeof(header)) != sizeof(header) ||
        read32(header) != HeaderMagic || read16(header + 4) != FormatVersion) {
      input.close();
      return false;
    }
    if (!input.seek(HeaderBytes)) {
      input.close();
      return false;
    }
    size_t records = 0, bytes = HeaderBytes;
    uint64_t sequence = 0;
    uint8_t frame[RecordOverheadBytes + MaxPayloadBytes]{};
    while (input.available()) {
      uint32_t body = 0;
      if (!readFrame(input, frame, body)) {
        input.close();
        return false;
      }
      ++records;
      bytes += 4 + body + 4;
      size_t pos = 4;
      RecordEnvelope envelope{};
      decodeEnvelope(frame, pos, envelope);
      if (envelope.sequence > sequence)
        sequence = envelope.sequence;
      if (records > Capacity || bytes > MaxFileBytes) {
        input.close();
        return false;
      }
    }
    input.close();
    recordCount_ = records;
    fileBytes_ = bytes;
    lastSequence_ = sequence;
    return true;
  }

  static bool readFrame(File& input, uint8_t* frame, uint32_t& body) {
    uint8_t lengthBytes[4]{};
    if (input.read(lengthBytes, sizeof(lengthBytes)) != sizeof(lengthBytes))
      return false;
    body = read32(lengthBytes);
    if (body < EnvelopeBytes || body > EnvelopeBytes + MaxPayloadBytes)
      return false;
    memcpy(frame, lengthBytes, sizeof(lengthBytes));
    if (input.read(frame + 4, body) != body ||
        input.read(frame + 4 + body, 4) != 4)
      return false;
    return checksum(frame, 4 + body) == read32(frame + 4 + body);
  }

  static void put16(uint8_t* out, size_t& pos, uint16_t value) {
    out[pos++] = uint8_t(value);
    out[pos++] = uint8_t(value >> 8);
  }
  static void put32(uint8_t* out, size_t& pos, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
      out[pos++] = uint8_t(value >> (i * 8));
  }
  static void put64(uint8_t* out, size_t& pos, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i)
      out[pos++] = uint8_t(value >> (i * 8));
  }
  static uint16_t read16(const uint8_t* in) {
    return uint16_t(in[0]) | (uint16_t(in[1]) << 8);
  }
  static uint32_t read32(const uint8_t* in) {
    return uint32_t(in[0]) | (uint32_t(in[1]) << 8) |
           (uint32_t(in[2]) << 16) | (uint32_t(in[3]) << 24);
  }
  static uint64_t read64(const uint8_t* in) {
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i)
      value |= uint64_t(in[i]) << (i * 8);
    return value;
  }
  static uint32_t checksum(const uint8_t* data, size_t length) {
    uint32_t value = 2166136261U;
    while (length--)
      value = (value ^ *data++) * 16777619U;
    return value;
  }

  static void encodeEnvelope(const RecordEnvelope& value, uint64_t sequence,
                             uint8_t* out, size_t& pos) {
    put16(out, pos, value.formatVersion);
    out[pos++] = uint8_t(value.kind);
    out[pos++] = uint8_t(value.severity);
    put64(out, pos, value.controllerEpoch.value);
    put64(out, pos, value.session.value);
    put64(out, pos, value.client.value);
    put64(out, pos, sequence);
    put64(out, pos, value.timestamp);
    put32(out, pos, value.correlation.request);
    put32(out, pos, value.correlation.event);
    put32(out, pos, value.correlation.parent);
    put64(out, pos, value.traceId);
    put32(out, pos, value.payloadType);
    out[pos++] = uint8_t(value.operation);
    out[pos++] = 0; out[pos++] = 0; out[pos++] = 0;
  }

  static void decodeEnvelope(const uint8_t* in, size_t& pos,
                             RecordEnvelope& value) {
    value.formatVersion = read16(in + pos); pos += 2;
    value.kind = EventKind(in[pos++]);
    value.severity = Severity(in[pos++]);
    value.controllerEpoch.value = read64(in + pos); pos += 8;
    value.session.value = read64(in + pos); pos += 8;
    value.client.value = read64(in + pos); pos += 8;
    value.sequence = read64(in + pos); pos += 8;
    value.timestamp = read64(in + pos); pos += 8;
    value.correlation.request = read32(in + pos); pos += 4;
    value.correlation.event = read32(in + pos); pos += 4;
    value.correlation.parent = read32(in + pos); pos += 4;
    value.traceId = read64(in + pos); pos += 8;
    value.payloadType = read32(in + pos); pos += 4;
    value.operation = OperationName(in[pos++]); pos += 3;
  }

  mutable File file_;
  mutable File readFile_;
  mutable size_t readIndex_ = 0;
  mutable bool readOpen_ = false;
  mutable uint64_t readRevision_ = 0;
  PhysicalSdOwner& owner_;
  ControllerEpoch epoch_{};
  StorageHealth health_ = StorageHealth::Fault;
  FailureReason lastFailure_ = FailureReason::None;
  mutable size_t recordCount_ = 0;
  mutable size_t fileBytes_ = 0;
  mutable uint64_t lastSequence_ = 0;
  mutable uint64_t revision_ = 0;
  mutable bool mounted_ = false;
  bool open_ = false;

  void reportPhysicalIoFailure(PhysicalSdOwner::Lease& lease) const {
    if (lease) lease.reportIoFailure();
    mounted_ = false;
  }

  void closeReadCursor() const {
    if (readFile_)
      readFile_.close();
    readIndex_ = 0;
    readOpen_ = false;
    readRevision_ = 0;
  }
};

} // namespace diagnostic
} // namespace pp
