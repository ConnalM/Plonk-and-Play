#pragma once

#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "pp/diagnostic_contract.h"

namespace pp {
namespace diagnostic {

// This is an explicitly development-only binary representation. It is not
// the production SD format or the eventual export encoding.
class DevelopmentFileStorage final : public DiagnosticStorage {
public:
  static constexpr uint32_t HeaderMagic = 0x44474C31u; // DGL1
  static constexpr uint32_t FrameMagic = 0x44475231u;  // DGR1
  static constexpr uint16_t FormatVersion = 2;
  static constexpr size_t DevelopmentFileMaxRecords = 64;
  static constexpr size_t DevelopmentFileMaxPayloadBytes = 4096;

  explicit DevelopmentFileStorage(std::string path) : path_(std::move(path)) {}

  StorageResult begin(ControllerEpoch epoch) override {
    closeFile();
    if (!epoch.valid()) return fail(StorageResult::Invalid, FailureReason::InvalidRequest);
    std::ofstream output(path_, std::ios::binary | std::ios::trunc);
    if (!output) return fail(StorageResult::Unavailable, FailureReason::BackendUnavailable);
    epoch_ = epoch;
    records_.clear();
    finalized_ = false;
    open_ = true;
    health_ = StorageHealth::Degraded;
    lastFailure_ = FailureReason::None;
    if (!writeHeader(output, false)) {
      open_ = false;
      return fail(StorageResult::WriteFailed, FailureReason::WriteFailure);
    }
    return StorageResult::Accepted;
  }

  StorageResult tryAppend(const RecordView& record, uint64_t sequence) override {
    if (!open_) return fail(StorageResult::Unavailable, FailureReason::BackendUnavailable);
    if (records_.size() >= DevelopmentFileMaxRecords) return fail(StorageResult::Full, FailureReason::StorageFull);
    if (record.payloadBytes > DevelopmentFileMaxPayloadBytes ||
        (record.payloadBytes != 0 && !record.payload)) {
      return fail(StorageResult::Invalid, FailureReason::PayloadFailure);
    }
    std::vector<uint8_t> frame;
    serializeFrame(record, sequence, frame);
    std::ofstream output(path_, std::ios::binary | std::ios::app);
    if (!output) return fail(StorageResult::WriteFailed, FailureReason::WriteFailure);
    output.write(reinterpret_cast<const char*>(frame.data()), static_cast<std::streamsize>(frame.size()));
    output.flush();
    if (!output) return fail(StorageResult::WriteFailed, FailureReason::WriteFailure);
    StoredRecord stored{};
    stored.envelope = record.envelope;
    stored.envelope.sequence = sequence;
    if (record.payloadBytes) stored.payload.assign(record.payload, record.payload + record.payloadBytes);
    records_.push_back(stored);
    std::fstream headerFile(path_, std::ios::binary | std::ios::in | std::ios::out);
    if (!headerFile || !writeHeader(headerFile, false)) return fail(StorageResult::WriteFailed, FailureReason::WriteFailure);
    return StorageResult::Accepted;
  }

  size_t count() const override { return records_.size(); }

  bool read(size_t index, RecordEnvelope& envelope, uint8_t* payload,
            size_t capacity, size_t& length) const override {
    length = 0;
    if (index >= records_.size()) return false;
    const StoredRecord& stored = records_[index];
    if (stored.payload.size() > capacity || (!stored.payload.empty() && !payload)) return false;
    envelope = stored.envelope;
    if (!stored.payload.empty()) memcpy(payload, stored.payload.data(), stored.payload.size());
    length = stored.payload.size();
    return true;
  }

  StorageStatus status() const override {
    StorageStatus result{};
    result.health = health_;
    result.backend = BackendKind::DevelopmentFile;
    result.lastFailure = lastFailure_;
    result.durable = finalized_;
    return result;
  }

  StorageResult recover() override {
    closeFile();
    std::ifstream input(path_, std::ios::binary);
    if (!input) return fail(StorageResult::Unavailable, FailureReason::BackendUnavailable);
    Header header{};
    if (!readHeader(input, header)) return fail(StorageResult::Corrupt, FailureReason::CorruptRecord);
    if (header.version != FormatVersion) return fail(StorageResult::UnsupportedVersion, FailureReason::UnsupportedVersion);
    if (header.count > DevelopmentFileMaxRecords) return fail(StorageResult::Corrupt, FailureReason::CorruptRecord);
    epoch_.value = header.epoch;
    records_.clear();
    for (uint32_t i = 0; i < header.count; ++i) {
      StoredRecord stored{};
      if (!readFrame(input, stored)) {
        records_.clear();
        return fail(StorageResult::Corrupt, FailureReason::CorruptRecord);
      }
      records_.push_back(stored);
    }
    open_ = true;
    finalized_ = (header.flags & 1u) != 0;
    if (!finalized_) {
      health_ = StorageHealth::Degraded;
      lastFailure_ = FailureReason::UnfinalizedSession;
      return StorageResult::Incomplete;
    }
    health_ = StorageHealth::DurableSd;
    lastFailure_ = FailureReason::None;
    return StorageResult::Accepted;
  }

  StorageResult close() override {
    if (!open_) return StorageResult::Accepted;
    std::fstream file(path_, std::ios::binary | std::ios::in | std::ios::out);
    if (!file) return fail(StorageResult::CommitFailed, FailureReason::CommitFailure);
    if (!writeHeader(file, true)) return fail(StorageResult::CommitFailed, FailureReason::CommitFailure);
    file.flush();
    if (!file) return fail(StorageResult::CommitFailed, FailureReason::CommitFailure);
    finalized_ = true;
    health_ = StorageHealth::DurableSd;
    lastFailure_ = FailureReason::None;
    open_ = false;
    return StorageResult::Accepted;
  }

  // Test-only mutation helpers used to prove corruption/version handling.
  bool rewriteVersionForTest(uint16_t version) {
    std::fstream file(path_, std::ios::binary | std::ios::in | std::ios::out);
    if (!file) return false;
    std::vector<uint8_t> header(24);
    file.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    if (!file) return false;
    header[4] = static_cast<uint8_t>(version);
    header[5] = static_cast<uint8_t>(version >> 8);
    const uint32_t sum = checksum(header.data(), 20);
    for (unsigned i = 0; i < 4; ++i) header[20 + i] = static_cast<uint8_t>(sum >> (i * 8));
    file.clear();
    file.seekp(0);
    file.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));
    file.flush();
    return static_cast<bool>(file);
  }

  bool corruptByteForTest(size_t offset) {
    std::fstream file(path_, std::ios::binary | std::ios::in | std::ios::out);
    if (!file) return false;
    file.seekg(static_cast<std::streamoff>(offset));
    char value = 0;
    file.read(&value, 1);
    if (!file) return false;
    file.seekp(static_cast<std::streamoff>(offset));
    value = static_cast<char>(value ^ 0x5A);
    file.write(&value, 1);
    file.flush();
    return static_cast<bool>(file);
  }

  bool finalized() const { return finalized_; }
  const std::string& path() const { return path_; }

private:
  struct Header { uint32_t magic = HeaderMagic; uint16_t version = FormatVersion; uint16_t flags = 0; uint64_t epoch = 0; uint32_t count = 0; uint32_t checksum = 0; };
  struct StoredRecord { RecordEnvelope envelope{}; std::vector<uint8_t> payload; };

  static uint32_t checksum(const uint8_t* data, size_t length) {
    uint32_t value = 2166136261u;
    while (length--) value = (value ^ *data++) * 16777619u;
    return value;
  }

  static void writeU16(std::ostream& stream, uint16_t value) { stream.put(static_cast<char>(value)); stream.put(static_cast<char>(value >> 8)); }
  static void writeU32(std::ostream& stream, uint32_t value) { for (unsigned i = 0; i < 4; ++i) stream.put(static_cast<char>(value >> (i * 8))); }
  static void writeU64(std::ostream& stream, uint64_t value) { for (unsigned i = 0; i < 8; ++i) stream.put(static_cast<char>(value >> (i * 8))); }
  static bool readU16(std::istream& stream, uint16_t& value) { unsigned char b[2]; stream.read(reinterpret_cast<char*>(b), 2); if (!stream) return false; value = uint16_t(b[0]) | uint16_t(b[1] << 8); return true; }
  static bool readU32(std::istream& stream, uint32_t& value) { unsigned char b[4]; stream.read(reinterpret_cast<char*>(b), 4); if (!stream) return false; value = uint32_t(b[0]) | (uint32_t(b[1]) << 8) | (uint32_t(b[2]) << 16) | (uint32_t(b[3]) << 24); return true; }
  static bool readU64(std::istream& stream, uint64_t& value) { unsigned char b[8]; stream.read(reinterpret_cast<char*>(b), 8); if (!stream) return false; value = 0; for (unsigned i = 0; i < 8; ++i) value |= uint64_t(b[i]) << (i * 8); return true; }

  bool writeHeader(std::ostream& stream, bool finalized) {
    std::vector<uint8_t> bytes;
    appendU32(bytes, HeaderMagic); appendU16(bytes, FormatVersion); appendU16(bytes, finalized ? 1u : 0u);
    appendU64(bytes, epoch_.value); appendU32(bytes, static_cast<uint32_t>(records_.size()));
    appendU32(bytes, checksum(bytes.data(), bytes.size()));
    stream.seekp(0);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(stream);
  }

  static bool readHeader(std::istream& stream, Header& header) {
    if (!readU32(stream, header.magic) || !readU16(stream, header.version) || !readU16(stream, header.flags) ||
        !readU64(stream, header.epoch) || !readU32(stream, header.count) || !readU32(stream, header.checksum)) return false;
    std::vector<uint8_t> bytes;
    appendU32(bytes, header.magic); appendU16(bytes, header.version); appendU16(bytes, header.flags); appendU64(bytes, header.epoch); appendU32(bytes, header.count);
    return header.magic == HeaderMagic && header.checksum == checksum(bytes.data(), bytes.size());
  }

  static void serializeFrame(const RecordView& record, uint64_t sequence, std::vector<uint8_t>& bytes) {
    appendU32(bytes, FrameMagic); appendU16(bytes, FormatVersion); appendU16(bytes, 0);
    appendU16(bytes, record.envelope.formatVersion); bytes.push_back(static_cast<uint8_t>(record.envelope.kind)); bytes.push_back(static_cast<uint8_t>(record.envelope.severity));
    appendU64(bytes, record.envelope.controllerEpoch.value); appendU64(bytes, record.envelope.session.value); appendU64(bytes, record.envelope.client.value); appendU64(bytes, sequence); appendU64(bytes, record.envelope.timestamp);
    appendU32(bytes, record.envelope.correlation.request); appendU32(bytes, record.envelope.correlation.event); appendU32(bytes, record.envelope.correlation.parent); appendU32(bytes, record.envelope.payloadType); bytes.push_back(static_cast<uint8_t>(record.envelope.operation));
    appendU32(bytes, static_cast<uint32_t>(record.payloadBytes));
    bytes.insert(bytes.end(), record.payload, record.payload + record.payloadBytes);
    appendU32(bytes, checksum(bytes.data(), bytes.size()));
  }

  static bool readFrame(std::istream& stream, StoredRecord& stored) {
    std::vector<uint8_t> bytes;
    uint32_t magic = 0; uint16_t version = 0, reserved = 0, envelopeVersion = 0; uint8_t kind = 0, severity = 0, operation = 0; uint64_t epoch = 0, session = 0, client = 0, sequence = 0, timestamp = 0; uint32_t request = 0, event = 0, parent = 0, payloadType = 0, payloadLength = 0;
    if (!readU32(stream, magic) || !readU16(stream, version) || !readU16(stream, reserved) || !readU16(stream, envelopeVersion)) return false;
    if (!stream.read(reinterpret_cast<char*>(&kind), 1) || !stream.read(reinterpret_cast<char*>(&severity), 1) || !readU64(stream, epoch) || !readU64(stream, session) || !readU64(stream, client) || !readU64(stream, sequence) || !readU64(stream, timestamp) || !readU32(stream, request) || !readU32(stream, event) || !readU32(stream, parent) || !readU32(stream, payloadType) || !stream.read(reinterpret_cast<char*>(&operation), 1) || !readU32(stream, payloadLength)) return false;
    if (magic != FrameMagic || version != FormatVersion || payloadLength > DevelopmentFileMaxPayloadBytes) return false;
    stored.payload.resize(payloadLength);
    if (payloadLength && !stream.read(reinterpret_cast<char*>(stored.payload.data()), static_cast<std::streamsize>(payloadLength))) return false;
    uint32_t supplied = 0; if (!readU32(stream, supplied)) return false;
    appendU32(bytes, magic); appendU16(bytes, version); appendU16(bytes, reserved); appendU16(bytes, envelopeVersion); bytes.push_back(kind); bytes.push_back(severity); appendU64(bytes, epoch); appendU64(bytes, session); appendU64(bytes, client); appendU64(bytes, sequence); appendU64(bytes, timestamp); appendU32(bytes, request); appendU32(bytes, event); appendU32(bytes, parent); appendU32(bytes, payloadType); bytes.push_back(operation); appendU32(bytes, payloadLength); bytes.insert(bytes.end(), stored.payload.begin(), stored.payload.end());
    if (supplied != checksum(bytes.data(), bytes.size())) return false;
    stored.envelope.formatVersion = envelopeVersion; stored.envelope.kind = static_cast<EventKind>(kind); stored.envelope.severity = static_cast<Severity>(severity); stored.envelope.controllerEpoch.value = epoch; stored.envelope.session.value = session; stored.envelope.client.value = client; stored.envelope.sequence = sequence; stored.envelope.timestamp = timestamp; stored.envelope.correlation.request = request; stored.envelope.correlation.event = event; stored.envelope.correlation.parent = parent; stored.envelope.payloadType = payloadType; stored.envelope.operation = static_cast<OperationName>(operation);
    return true;
  }

  static void appendU16(std::vector<uint8_t>& bytes, uint16_t value) { bytes.push_back(static_cast<uint8_t>(value)); bytes.push_back(static_cast<uint8_t>(value >> 8)); }
  static void appendU32(std::vector<uint8_t>& bytes, uint32_t value) { for (unsigned i = 0; i < 4; ++i) bytes.push_back(static_cast<uint8_t>(value >> (i * 8))); }
  static void appendU64(std::vector<uint8_t>& bytes, uint64_t value) { for (unsigned i = 0; i < 8; ++i) bytes.push_back(static_cast<uint8_t>(value >> (i * 8))); }
  StorageResult fail(StorageResult result, FailureReason reason) { health_ = StorageHealth::Fault; lastFailure_ = reason; return result; }
  void closeFile() { open_ = false; }

  std::string path_;
  std::vector<StoredRecord> records_;
  ControllerEpoch epoch_{};
  StorageHealth health_ = StorageHealth::Fault;
  mutable FailureReason lastFailure_ = FailureReason::None;
  bool open_ = false;
  bool finalized_ = false;
};

class FailureInjectionStorage final : public DiagnosticStorage {
public:
  explicit FailureInjectionStorage(DiagnosticStorage& inner) : inner_(inner) {}

  StorageResult begin(ControllerEpoch epoch) override { return consume(beginFailure_, [&] { return inner_.begin(epoch); }); }
  StorageResult tryAppend(const RecordView& record, uint64_t sequence) override { return consume(appendFailure_, [&] { return inner_.tryAppend(record, sequence); }); }
  size_t count() const override { return inner_.count(); }
  bool read(size_t index, RecordEnvelope& envelope, uint8_t* payload, size_t capacity, size_t& length) const override {
    if (readFailure_) { readFailure_ = false; lastFailure_ = FailureReason::CorruptRecord; return false; }
    return inner_.read(index, envelope, payload, capacity, length);
  }
  StorageStatus status() const override {
    StorageStatus result = inner_.status();
    result.backend = BackendKind::FailureInjection;
    if (lastFailure_ != FailureReason::None) { result.health = StorageHealth::Fault; result.lastFailure = lastFailure_; }
    return result;
  }
  StorageResult recover() override { return consume(recoverFailure_, [&] { return inner_.recover(); }); }
  StorageResult close() override { return consume(closeFailure_, [&] { return inner_.close(); }); }

  void failNextBegin(StorageResult result) { beginFailure_ = result; }
  void failNextAppend(StorageResult result) { appendFailure_ = result; }
  void failNextRecover(StorageResult result) { recoverFailure_ = result; }
  void failNextClose(StorageResult result) { closeFailure_ = result; }
  void failNextRead() const { readFailure_ = true; }

private:
  template <typename Function>
  StorageResult consume(StorageResult& injected, Function function) {
    if (injected != StorageResult::Accepted) { const StorageResult result = injected; injected = StorageResult::Accepted; lastFailure_ = reasonFor(result); return result; }
    return function();
  }
  static FailureReason reasonFor(StorageResult result) {
    switch (result) {
      case StorageResult::Unavailable: return FailureReason::BackendUnavailable;
      case StorageResult::Full: return FailureReason::StorageFull;
      case StorageResult::WriteFailed: return FailureReason::WriteFailure;
      case StorageResult::CommitFailed: return FailureReason::CommitFailure;
      case StorageResult::Corrupt: return FailureReason::CorruptRecord;
      case StorageResult::UnsupportedVersion: return FailureReason::UnsupportedVersion;
      case StorageResult::Incomplete: return FailureReason::UnfinalizedSession;
      case StorageResult::Invalid: return FailureReason::InvalidRequest;
      default: return FailureReason::RecorderFault;
    }
  }
  DiagnosticStorage& inner_;
  mutable bool readFailure_ = false;
  mutable FailureReason lastFailure_ = FailureReason::None;
  StorageResult beginFailure_ = StorageResult::Accepted;
  StorageResult appendFailure_ = StorageResult::Accepted;
  StorageResult recoverFailure_ = StorageResult::Accepted;
  StorageResult closeFailure_ = StorageResult::Accepted;
};

} // namespace diagnostic
} // namespace pp

