#pragma once

#include <stddef.h>
#include <stdint.h>

#include "diagnostic_contract.h"

namespace pp {
namespace diagnostic {

// Phase 7 development retrieval is deliberately cursor based.  A caller
// supplies the payload buffer, so reading never requires loading the log into
// one RAM object and never mutates the recorder merely because it was read.
struct RetrievalChunk {
  size_t cursor = 0;
  size_t nextCursor = 0;
  size_t retainedRecords = 0;
  bool end = false;
  bool available = false;
  bool readFailed = false;
  bool evidenceIncomplete = false;
  RecordEnvelope envelope{};
  size_t payloadBytes = 0;
};

class DiagnosticRetrieval final {
public:
  // This is a development transport bound, not a product/export capacity.
  static constexpr size_t MaxPayloadBytes = 4096;

  static bool next(const DiagnosticRecorder& recorder, size_t cursor,
                   uint8_t* payload, size_t payloadCapacity,
                   RetrievalChunk& chunk) {
    chunk = RetrievalChunk{};
    chunk.cursor = cursor;
    chunk.retainedRecords = recorder.count();
    SessionStatus session{};
    if (recorder.status(session)) chunk.evidenceIncomplete = session.evidenceIncomplete;
    if (cursor >= chunk.retainedRecords) {
      chunk.nextCursor = cursor;
      chunk.end = true;
      return true;
    }
    if (payloadCapacity > MaxPayloadBytes) payloadCapacity = MaxPayloadBytes;
    size_t length = 0;
    if (!recorder.read(cursor, chunk.envelope, payload, payloadCapacity, length)) {
      chunk.nextCursor = cursor;
      chunk.readFailed = true;
      chunk.evidenceIncomplete = true;
      return false;
    }
    chunk.available = true;
    chunk.payloadBytes = length;
    chunk.nextCursor = cursor + 1;
    chunk.end = chunk.nextCursor >= chunk.retainedRecords;
    return true;
  }
};

} // namespace diagnostic
} // namespace pp
