#pragma once

#include "taster_link_gate.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pp::taster_link::wire {

// Host protocol slice. Explicit big-endian fields, no native struct ABI.
// This is post-handshake traffic only. The caller must supply a fresh,
// authenticated Session and a distinct session-bound MAC key per direction.
// The production handshake, key lifecycle and transport are NOT in this file.
constexpr uint8_t Version = 1;
constexpr size_t HeaderSize = 32;
constexpr size_t TagSize = 32; // HMAC-SHA256, full tag
constexpr size_t MaxPayload = 64;
constexpr size_t MaxFrame = HeaderSize + MaxPayload + TagSize;

enum class Direction : uint8_t { ToCore, ToTaster };
enum class Kind : uint8_t { Command = 1, Result = 2, ControlState = 3, Ping = 4, Pong = 5 };

class Mac {
public:
  virtual ~Mac() = default;
  virtual bool sign(const uint8_t* bytes, size_t length, uint8_t* tag) const = 0;
  // Provider must verify the entire 32-byte tag using a constant-time
  // comparison and fail closed on cryptographic errors.
  virtual bool verify(const uint8_t* bytes, size_t length, const uint8_t* tag) const = 0;
};

struct Frame {
  Kind kind = Kind::Command;
  Session session{};
  uint64_t sequence = 0;
  uint16_t length = 0;
  std::array<uint8_t, MaxPayload> payload{};
};

struct Result {
  uint64_t commandId = 0;
  Status status = Status::Unknown;
};

// This is only the command-facing control context, NOT a race display
// snapshot. Full lane/timing/presentation state needs its own reviewed schema.
struct ControlState {
  Context context{};
  uint64_t coreTimeUs = 0;
};
struct Ping { uint64_t token = 0; uint64_t tasterSendUs = 0; };
struct Pong {
  uint64_t token = 0;
  uint64_t tasterSendUs = 0;
  uint64_t coreReceiveUs = 0;
  uint64_t coreSendUs = 0;
};

inline void put(uint8_t*& p, uint64_t value, unsigned bytes) {
  for (unsigned i = bytes; i; --i) *p++ = static_cast<uint8_t>(value >> (8u * (i-1u)));
}
inline uint64_t get(const uint8_t*& p, unsigned bytes) {
  uint64_t value = 0;
  for (unsigned i = 0; i < bytes; ++i) value = (value << 8u) | *p++;
  return value;
}
inline uint16_t payloadSize(Kind kind) {
  switch (kind) {
    case Kind::Command: return 49;
    case Kind::Result: return 9;
    case Kind::ControlState: return 38;
    case Kind::Ping: return 16;
    case Kind::Pong: return 32;
    default: return 0;
  }
}
inline bool allowedDirection(Kind kind, Direction d) {
  if (d == Direction::ToCore) return kind == Kind::Command || kind == Kind::Ping;
  if (d == Direction::ToTaster)
    return kind == Kind::Result || kind == Kind::ControlState || kind == Kind::Pong;
  return false;
}
inline Frame init(Kind kind, Session session, uint64_t sequence) {
  Frame f{};
  f.kind = kind; f.session = session; f.sequence = sequence;
  f.length = payloadSize(kind);
  return f;
}
inline bool hasShape(const Frame& f, Kind kind) {
  return f.kind == kind && f.session.valid() && f.sequence &&
         f.length == payloadSize(kind) && f.length && f.length <= MaxPayload;
}

inline Frame command(const Command& q, uint64_t sequence) {
  Frame f = init(Kind::Command, q.session, sequence);
  auto* p = f.payload.data();
  put(p,q.id,8); put(p,q.authorityGeneration,8); put(p,q.lifecycleRevision,8);
  put(p,q.raceId,8); put(p,q.proposalRevision,4); put(p,q.expiresAtUs,8);
  put(p,static_cast<uint8_t>(q.action),1); put(p,static_cast<uint8_t>(q.mode),1);
  put(p,q.value,2); put(p,q.confirmed ? 1u : 0u,1);
  return f;
}
inline bool readCommand(const Frame& f, Command& out) {
  if (!hasShape(f,Kind::Command)) return false;
  Command q{}; q.session = f.session;
  const auto* p = f.payload.data();
  q.id=get(p,8); q.authorityGeneration=get(p,8); q.lifecycleRevision=get(p,8);
  q.raceId=get(p,8); q.proposalRevision=static_cast<uint32_t>(get(p,4));
  q.expiresAtUs=get(p,8);
  const auto action=get(p,1), mode=get(p,1);
  q.value=static_cast<uint16_t>(get(p,2)); const auto confirmed=get(p,1);
  if (!q.id || action > static_cast<uint8_t>(Action::RaceAgain) ||
      mode > static_cast<uint8_t>(Mode::Timed) || confirmed > 1) return false;
  q.action=static_cast<Action>(action); q.mode=static_cast<Mode>(mode);
  q.confirmed=confirmed != 0;
  // Sporting semantics, setup bounds and authority are the CommandGate's job.
  out=q; return true;
}
inline Frame result(Session session, uint64_t sequence, Result r) {
  Frame f=init(Kind::Result,session,sequence);
  auto* p=f.payload.data();
  put(p,r.commandId,8); put(p,static_cast<uint8_t>(r.status),1);
  return f;
}
inline bool readResult(const Frame& f, Result& out) {
  if (!hasShape(f,Kind::Result)) return false;
  const auto* p=f.payload.data();
  Result r{}; r.commandId=get(p,8); const auto s=get(p,1);
  if (!r.commandId || s > static_cast<uint8_t>(Status::WrongSession)) return false;
  r.status=static_cast<Status>(s); out=r; return true;
}
inline Frame controlState(const ControlState& s, uint64_t sequence) {
  Frame f=init(Kind::ControlState,s.context.session,sequence);
  auto* p=f.payload.data();
  put(p,s.context.authorityGeneration,8); put(p,s.context.lifecycleRevision,8);
  put(p,s.context.raceId,8); put(p,s.context.proposalRevision,4);
  put(p,static_cast<uint8_t>(s.context.lifecycle),1);
  put(p,s.context.tasterAllowed ? 1u : 0u,1); put(p,s.coreTimeUs,8);
  return f;
}
inline bool readControlState(const Frame& f, ControlState& out) {
  if (!hasShape(f,Kind::ControlState)) return false;
  ControlState s{}; s.context.session=f.session;
  const auto* p=f.payload.data();
  s.context.authorityGeneration=get(p,8); s.context.lifecycleRevision=get(p,8);
  s.context.raceId=get(p,8); s.context.proposalRevision=static_cast<uint32_t>(get(p,4));
  const auto lifecycle=get(p,1), allowed=get(p,1); s.coreTimeUs=get(p,8);
  if (lifecycle > static_cast<uint8_t>(Lifecycle::Other) || allowed > 1) return false;
  s.context.lifecycle=static_cast<Lifecycle>(lifecycle); s.context.tasterAllowed=allowed != 0;
  out=s; return true;
}
inline Frame ping(Session session, uint64_t sequence, Ping v) {
  Frame f=init(Kind::Ping,session,sequence);
  auto* p=f.payload.data(); put(p,v.token,8); put(p,v.tasterSendUs,8); return f;
}
inline bool readPing(const Frame& f, Ping& out) {
  if (!hasShape(f,Kind::Ping)) return false;
  const auto* p=f.payload.data(); Ping v{}; v.token=get(p,8); v.tasterSendUs=get(p,8);
  if (!v.token) return false;
  out=v; return true;
}
inline Frame pong(Session session, uint64_t sequence, Pong v) {
  Frame f=init(Kind::Pong,session,sequence);
  auto* p=f.payload.data();
  put(p,v.token,8); put(p,v.tasterSendUs,8); put(p,v.coreReceiveUs,8); put(p,v.coreSendUs,8);
  return f;
}
inline bool readPong(const Frame& f, Pong& out) {
  if (!hasShape(f,Kind::Pong)) return false;
  const auto* p=f.payload.data();
  Pong v{}; v.token=get(p,8); v.tasterSendUs=get(p,8);
  v.coreReceiveUs=get(p,8); v.coreSendUs=get(p,8);
  if (!v.token || v.coreSendUs < v.coreReceiveUs) return false;
  out=v; return true;
}
inline bool validPayload(const Frame& f) {
  switch (f.kind) {
    case Kind::Command: { Command q; return readCommand(f,q); }
    case Kind::Result: { Result r; return readResult(f,r); }
    case Kind::ControlState: { ControlState s; return readControlState(f,s); }
    case Kind::Ping: { Ping v; return readPing(f,v); }
    case Kind::Pong: { Pong v; return readPong(f,v); }
    default: return false;
  }
}
inline bool readHeader(const uint8_t* bytes, Frame& out) {
  const auto* p=bytes;
  if (get(p,4) != 0x5050544cULL || get(p,1) != Version) return false; // PPTL
  Frame f{}; f.kind=static_cast<Kind>(get(p,1));
  f.length=static_cast<uint16_t>(get(p,2));
  f.session.epoch=get(p,8); f.session.link=get(p,8); f.sequence=get(p,8);
  if (!f.session.valid() || !f.sequence || !f.length || f.length > MaxPayload ||
      f.length != payloadSize(f.kind)) return false;
  out=f; return true;
}

struct Packet { std::array<uint8_t,MaxFrame> bytes{}; size_t length=0; };

inline bool seal(const Frame& f, Direction direction, const Mac& mac, Packet& out) {
  out.length=0;
  if (!allowedDirection(f.kind,direction) || !validPayload(f)) return false;
  auto* p=out.bytes.data();
  put(p,0x5050544cULL,4); put(p,Version,1); put(p,static_cast<uint8_t>(f.kind),1);
  put(p,f.length,2); put(p,f.session.epoch,8); put(p,f.session.link,8); put(p,f.sequence,8);
  std::memcpy(p,f.payload.data(),f.length);
  const size_t signedLength=HeaderSize+f.length;
  if (!mac.sign(out.bytes.data(),signedLength,out.bytes.data()+signedLength)) return false;
  out.length=signedLength+TagSize;
  return true;
}

enum class Read { NeedMore, Ready, Fatal };

// A fresh decoder per authenticated connection and receive direction. Feed
// every received byte in order; TCP may split or coalesce frames. No frame
// escapes before MAC/shape/sequence validation. Any failure poisons the
// connection; do not scan for magic or resume in the middle of a stream.
class StreamDecoder {
public:
  StreamDecoder(Session session, Direction direction, const Mac& mac)
      : session_(session), direction_(direction), mac_(mac) {}
  StreamDecoder(const StreamDecoder&) = delete;
  StreamDecoder& operator=(const StreamDecoder&) = delete;
  Read push(uint8_t byte, Frame& out) {
    if (failed_ || exhausted_ || used_ >= bytes_.size()) return fail();
    bytes_[used_++]=byte;
    if (used_ == HeaderSize) {
      Frame header;
      if (!readHeader(bytes_.data(),header) || !(header.session == session_) ||
          header.sequence != expected_ || !allowedDirection(header.kind,direction_))
        return fail();
      total_=HeaderSize+header.length+TagSize;
    }
    if (!total_ || used_ < total_) return Read::NeedMore;
    const size_t signedLength=total_-TagSize;
    if (!mac_.verify(bytes_.data(),signedLength,bytes_.data()+signedLength)) return fail();
    Frame f;
    if (!readHeader(bytes_.data(),f)) return fail();
    std::memcpy(f.payload.data(),bytes_.data()+HeaderSize,f.length);
    if (!validPayload(f)) return fail();
    out=f;
    used_=0; total_=0;
    if (expected_ == UINT64_MAX) exhausted_=true;
    else ++expected_;
    return Read::Ready;
  }
  bool partial() const { return used_ != 0; }
  bool failed() const { return failed_; }
private:
  Read fail() { failed_=true; return Read::Fatal; }
  const Session session_;
  const Direction direction_;
  const Mac& mac_;
  std::array<uint8_t,MaxFrame> bytes_{};
  size_t used_=0,total_=0;
  uint64_t expected_=1;
  bool failed_=false,exhausted_=false;
};

} // namespace pp::taster_link::wire
