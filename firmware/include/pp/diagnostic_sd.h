#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <string.h>

#include "diagnostic_contract.h"

namespace pp {
namespace diagnostic {

// Phase 8 hardware-test backend. This is a deliberately provisional,
// session-scoped file representation for the ESP32-8048S070 microSD card. It
// is not the production diagnostic filesystem or export format.
class Esp32SpiSdDiagnosticStorage final : public DiagnosticStorage {
public:
  static constexpr int CsPin = 10;
  static constexpr int MosiPin = 11;
  static constexpr int SckPin = 12;
  static constexpr int MisoPin = 13;
  static constexpr uint32_t SpiFrequency = 20000000U;
  static constexpr size_t MaxPayloadBytes = 512;
  // Explicit wire fields occupy 72 bytes; this intentionally does not use
  // sizeof(RecordEnvelope), whose C++ padding is not part of the format.
  static constexpr size_t EnvelopeBytes = 72;
  static constexpr size_t RecordBytes = 4 + EnvelopeBytes + MaxPayloadBytes + 4;

  StorageResult begin(ControllerEpoch epoch) override {
    return open(epoch, true);
  }

  // Used only by the hardware probe to reconstruct the file written by a
  // previous backend instance without creating a second diagnostic format.
  StorageResult recoverExisting(ControllerEpoch epoch) {
    return open(epoch, false);
  }

  StorageResult tryAppend(const RecordView& record, uint64_t sequence) override {
    if (!open_ || !file_) return fail(StorageResult::Unavailable, FailureReason::BackendUnavailable);
    if (nextAppendFailure_ != StorageResult::Accepted) {
      const StorageResult result = nextAppendFailure_;
      nextAppendFailure_ = StorageResult::Accepted;
      return fail(result, result == StorageResult::CommitFailed ? FailureReason::CommitFailure : FailureReason::WriteFailure);
    }
    if (record.payloadBytes > MaxPayloadBytes || (record.payloadBytes && !record.payload))
      return fail(StorageResult::Invalid, FailureReason::InvalidRequest);
    uint8_t bytes[RecordBytes]{};
    size_t used = 0;
    put32(bytes, used, static_cast<uint32_t>(EnvelopeBytes + record.payloadBytes));
    encodeEnvelope(record.envelope, sequence, bytes, used);
    if (record.payloadBytes) { memcpy(bytes + used, record.payload, record.payloadBytes); used += record.payloadBytes; }
    const uint32_t checksum = hash(bytes, used);
    put32(bytes, used, checksum);
    const size_t written = file_.write(bytes, used);
    file_.flush();
    if (written != used) return fail(StorageResult::WriteFailed, FailureReason::WriteFailure);
    return StorageResult::Accepted;
  }

  size_t count() const override { return scan(false); }

  bool read(size_t wanted, RecordEnvelope& envelope, uint8_t* payload,
            size_t capacity, size_t& length) const override {
    length = 0;
    if (!open_ || !file_) return false;
    File input = SD.open(path_, FILE_READ);
    if (!input) return false;
    size_t index = 0;
    uint8_t bytes[RecordBytes]{};
    while (readRecord(input, bytes)) {
      const uint32_t payloadBytes = read32(bytes);
      if (index++ != wanted) continue;
      if (payloadBytes < EnvelopeBytes || payloadBytes - EnvelopeBytes > MaxPayloadBytes ||
          payloadBytes - EnvelopeBytes > capacity || (payloadBytes != EnvelopeBytes && !payload)) {
        input.close();
        return false;
      }
      size_t offset = 4;
      decodeEnvelope(bytes, offset, envelope);
      const size_t actual = payloadBytes - EnvelopeBytes;
      if (actual) memcpy(payload, bytes + offset, actual);
      length = actual;
      input.close();
      return true;
    }
    input.close();
    return false;
  }

  StorageStatus status() const override {
    StorageStatus result{};
    result.health = health_;
    result.backend = BackendKind::ProductionSd;
    result.lastFailure = lastFailure_;
    result.durable = mounted_ && health_ == StorageHealth::DurableSd;
    return result;
  }

  StorageResult recover() override {
    if (!mounted_) return fail(StorageResult::Unavailable, FailureReason::BackendUnavailable);
    const size_t valid = scan(true);
    if (invalid_) {
      health_ = StorageHealth::Degraded;
      lastFailure_ = invalidReason_;
      return StorageResult::Corrupt;
    }
    health_ = StorageHealth::DurableSd;
    lastFailure_ = FailureReason::None;
    return valid ? StorageResult::Accepted : StorageResult::Incomplete;
  }

  StorageResult close() override {
    if (!open_) return StorageResult::Accepted;
    uint8_t footer[12]{}; size_t used = 0;
    put32(footer, used, FooterMagic); put32(footer, used, static_cast<uint32_t>(scan(false)));
    put32(footer, used, hash(footer, 8));
    if (file_.write(footer, used) != used) return fail(StorageResult::CommitFailed, FailureReason::CommitFailure);
    file_.flush();
    file_.close();
    open_ = false;
    finalized_ = true;
    return StorageResult::Accepted;
  }

  bool mounted() const { return mounted_; }
  const char* path() const { return path_; }
  bool finalized() const { return finalized_; }
  uint64_t cardBytes() const { return mounted_ ? SD.cardSize() : 0; }
  uint64_t totalBytes() const { return mounted_ ? SD.totalBytes() : 0; }
  uint64_t usedBytes() const { return mounted_ ? SD.usedBytes() : 0; }
  void failNextAppend(StorageResult result) { nextAppendFailure_ = result; }
  void failNextRead() { nextReadFailure_ = true; }

private:
  static constexpr uint32_t HeaderMagic = 0x44313538U; // D158
  static constexpr uint16_t FormatVersion = 1;
  static constexpr uint32_t FooterMagic = 0x46313538U; // F158

  StorageResult open(ControllerEpoch epoch, bool fresh) {
    close();
    spi_.begin(SckPin, MisoPin, MosiPin, CsPin);
    mounted_ = SD.begin(CsPin, spi_, SpiFrequency);
    if (!mounted_) return fail(StorageResult::Unavailable, FailureReason::BackendUnavailable);
    epoch_ = epoch;
    snprintf(path_, sizeof(path_), "/diag8-%llu.bin", static_cast<unsigned long long>(epoch.value));
    if (fresh && SD.exists(path_)) SD.remove(path_);
    file_ = SD.open(path_, FILE_WRITE);
    if (!file_) return fail(StorageResult::Unavailable, FailureReason::BackendUnavailable);
    open_ = true;
    finalized_ = false;
    invalid_ = false;
    invalidReason_ = FailureReason::None;
    health_ = StorageHealth::DurableSd;
    lastFailure_ = FailureReason::None;
    if (fresh) {
      uint8_t header[16]{}; size_t used = 0;
      put32(header, used, HeaderMagic); put16(header, used, FormatVersion); put16(header, used, 0);
      put64(header, used, epoch.value);
      if (file_.write(header, used) != used) return fail(StorageResult::WriteFailed, FailureReason::WriteFailure);
      file_.flush();
      return StorageResult::Accepted;
    }
    return recover();
  }

  StorageResult fail(StorageResult result, FailureReason reason) {
    health_ = StorageHealth::Fault; lastFailure_ = reason; return result;
  }

  size_t scan(bool recoverMode) const {
    invalid_ = false; finalized_ = false;
    if (!open_ || !file_) return 0;
    File input = SD.open(path_, FILE_READ);
    if (!input) { invalid_ = true; invalidReason_ = FailureReason::BackendUnavailable; return 0; }
    uint8_t header[16]{};
    if (input.read(header, sizeof(header)) != sizeof(header) || read32(header) != HeaderMagic || read16(header + 4) != FormatVersion) {
      input.close(); invalid_ = true; invalidReason_ = FailureReason::UnsupportedVersion; return 0;
    }
    size_t records = 0; uint8_t bytes[RecordBytes]{};
    while (readRecord(input, bytes)) ++records;
    if (input.available()) { invalid_ = true; invalidReason_ = FailureReason::CorruptRecord; }
    input.close();
    if (recoverMode && !finalized_) { invalid_ = true; invalidReason_ = FailureReason::UnfinalizedSession; }
    return records;
  }

  bool readRecord(File& input, uint8_t* bytes) const {
    if (nextReadFailure_) { nextReadFailure_ = false; return false; }
    uint8_t lengthBytes[4]{};
    if (input.read(lengthBytes, 4) != 4) return false;
    const uint32_t body = read32(lengthBytes);
    if (body == FooterMagic) {
      uint8_t footer[8]{};
      if (input.read(footer, sizeof(footer)) != sizeof(footer)) {
        invalid_ = true; invalidReason_ = FailureReason::CorruptRecord; return false;
      }
      uint8_t check[8]{}; memcpy(check, lengthBytes, 4); memcpy(check + 4, footer, 4);
      if (hash(check, sizeof(check)) != read32(footer + 4)) {
        invalid_ = true; invalidReason_ = FailureReason::CorruptRecord; return false;
      }
      finalized_ = true;
      return false;
    }
    if (body < EnvelopeBytes || body > EnvelopeBytes + MaxPayloadBytes) { invalid_ = true; invalidReason_ = FailureReason::CorruptRecord; return false; }
    memcpy(bytes, lengthBytes, 4);
    if (input.read(bytes + 4, body) != body || input.read(bytes + 4 + body, 4) != 4) { invalid_ = true; invalidReason_ = FailureReason::UnfinalizedSession; return false; }
    if (hash(bytes, 4 + body) != read32(bytes + 4 + body)) { invalid_ = true; invalidReason_ = FailureReason::CorruptRecord; return false; }
    return true;
  }

  static void put16(uint8_t* out, size_t& pos, uint16_t value) { out[pos++] = uint8_t(value); out[pos++] = uint8_t(value >> 8); }
  static void put32(uint8_t* out, size_t& pos, uint32_t value) { for (unsigned i = 0; i < 4; ++i) out[pos++] = uint8_t(value >> (i * 8)); }
  static void put64(uint8_t* out, size_t& pos, uint64_t value) { for (unsigned i = 0; i < 8; ++i) out[pos++] = uint8_t(value >> (i * 8)); }
  static uint16_t read16(const uint8_t* in) { return uint16_t(in[0]) | (uint16_t(in[1]) << 8); }
  static uint32_t read32(const uint8_t* in) { return uint32_t(in[0]) | (uint32_t(in[1]) << 8) | (uint32_t(in[2]) << 16) | (uint32_t(in[3]) << 24); }
  static uint64_t read64(const uint8_t* in) { uint64_t value = 0; for (unsigned i = 0; i < 8; ++i) value |= uint64_t(in[i]) << (i * 8); return value; }
  static uint32_t hash(const uint8_t* data, size_t length) { uint32_t h = 2166136261U; while (length--) h = (h ^ *data++) * 16777619U; return h; }

  static void encodeEnvelope(const RecordEnvelope& value, uint64_t sequence, uint8_t* out, size_t& pos) {
    put16(out, pos, value.formatVersion); out[pos++] = uint8_t(value.kind); out[pos++] = uint8_t(value.severity);
    put64(out, pos, value.controllerEpoch.value); put64(out, pos, value.session.value); put64(out, pos, value.client.value);
    put64(out, pos, sequence); put64(out, pos, value.timestamp); put32(out, pos, value.correlation.request); put32(out, pos, value.correlation.event); put32(out, pos, value.correlation.parent);
    put64(out, pos, value.traceId); put32(out, pos, value.payloadType); out[pos++] = uint8_t(value.operation); out[pos++] = 0; out[pos++] = 0; out[pos++] = 0;
  }
  static void decodeEnvelope(const uint8_t* in, size_t& pos, RecordEnvelope& value) {
    value.formatVersion = read16(in + pos); pos += 2; value.kind = EventKind(in[pos++]); value.severity = Severity(in[pos++]);
    value.controllerEpoch.value = read64(in + pos); pos += 8; value.session.value = read64(in + pos); pos += 8; value.client.value = read64(in + pos); pos += 8;
    value.sequence = read64(in + pos); pos += 8; value.timestamp = read64(in + pos); pos += 8; value.correlation.request = read32(in + pos); pos += 4; value.correlation.event = read32(in + pos); pos += 4; value.correlation.parent = read32(in + pos); pos += 4;
    value.traceId = read64(in + pos); pos += 8; value.payloadType = read32(in + pos); pos += 4; value.operation = OperationName(in[pos++]); pos += 3;
  }

  mutable File file_;
  mutable bool nextReadFailure_ = false;
  mutable bool invalid_ = false;
  mutable FailureReason invalidReason_ = FailureReason::None;
  SPIClass spi_{FSPI};
  ControllerEpoch epoch_{};
  char path_[48]{};
  StorageHealth health_ = StorageHealth::Fault;
  FailureReason lastFailure_ = FailureReason::None;
  StorageResult nextAppendFailure_ = StorageResult::Accepted;
  bool mounted_ = false, open_ = false;
  mutable bool finalized_ = false;
};

} // namespace diagnostic
} // namespace pp
