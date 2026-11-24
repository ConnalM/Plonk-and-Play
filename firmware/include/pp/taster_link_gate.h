#pragma once

#include <cstdint>

namespace pp::taster_link {

// S3a host-only command gate prototype. No socket, crypto, Bus, UI or
// production call sites. Callers MUST authenticate the session and each
// message before entry, and must run all gate methods on one owner thread.
// Neither receipt nor dispatch is an authoritative Race Control result.
enum class Action : uint8_t { Setup, Start, Pause, Resume, Abandon, RaceAgain };
enum class Mode : uint8_t { None, Laps, Timed };
enum class Lifecycle : uint8_t { Ready, Starting, Racing, Paused, Finished, Other };
enum class Status : uint8_t {
  Unknown, Queued, InFlight, Accepted, Rejected, Stale, Expired,
  Forbidden, Invalid, OldId, Conflict, Busy, WrongSession
};
struct Session {
  uint64_t epoch = 0;
  uint64_t link = 0;
  bool valid() const { return epoch && link; }
  bool operator==(const Session& other) const {
    return epoch == other.epoch && link == other.link;
  }
};
struct Context {
  Session session{};
  uint64_t authorityGeneration = 0;
  uint64_t lifecycleRevision = 0;
  uint64_t raceId = 0;
  uint32_t proposalRevision = 0;
  Lifecycle lifecycle = Lifecycle::Other;
  bool tasterAllowed = false; // Core computes mode/owner/capability policy.
};
struct Command {
  Session session{};
  uint64_t id = 0;
  uint64_t authorityGeneration = 0;
  uint64_t lifecycleRevision = 0;
  uint64_t raceId = 0;
  uint32_t proposalRevision = 0;
  uint64_t expiresAtUs = 0; // Core monotonic clock domain
  Action action = Action::Start;
  Mode mode = Mode::None;
  uint16_t value = 0; // active setup value only; Core preserves inactive value
  bool confirmed = false;
  bool operator==(const Command& x) const {
    return session == x.session && id == x.id &&
           authorityGeneration == x.authorityGeneration &&
           lifecycleRevision == x.lifecycleRevision && raceId == x.raceId &&
           proposalRevision == x.proposalRevision && expiresAtUs == x.expiresAtUs &&
           action == x.action && mode == x.mode && value == x.value &&
           confirmed == x.confirmed;
  }
};
class CommandGate {
public:
  // Provisional engineering limit, not a frozen sporting tolerance.
  static constexpr uint64_t MaxFutureUs = 500000;

  // Only a verified fresh handshake may call open(). Session IDs must not be
  // reused even after a Core/Taster reboot; the handshake owns that guarantee.
  bool open(Session fresh) {
    if (!fresh.valid() || fresh == session_) return false;
    disconnect();
    session_ = fresh;
    connected_ = true;
    highWater_ = 0;
    return true;
  }

  // A queued command is cancelled; a command already dispatched may have
  // executed. Discard local knowledge rather than claim it did not execute.
  void disconnect() {
    connected_ = false;
    last_ = {};
    status_ = Status::Unknown;
  }

  Status submit(const Command& cmd, const Context& current, uint64_t nowUs) {
    if (!connected_ || !(cmd.session == session_) ||
        !(current.session == session_)) return Status::WrongSession;
    if (!cmd.id) return Status::Invalid;
    if (last_.id == cmd.id)
      return last_ == cmd ? status_ : Status::Conflict;
    if (cmd.id <= highWater_) return Status::OldId;
    // Retire each new ID immediately, including BUSY; a later retry cannot
    // turn the earlier rejection into execution. Keep the in-flight record.
    highWater_ = cmd.id;
    if (status_ == Status::Queued || status_ == Status::InFlight)
      return Status::Busy;
    last_ = cmd;
    status_ = validate(cmd, current, nowUs);
    return status_;
  }

  // Revalidate at main-loop dequeue. The caller must ensure these same
  // guards remain valid through actual execution; a queued Bus publish
  // alone is NOT that guarantee. No dispatch occurs twice per ID.
  bool take(const Context& current, uint64_t nowUs, Command& out) {
    if (!connected_ || status_ != Status::Queued) return false;
    status_ = validate(last_, current, nowUs);
    if (status_ != Status::Queued) return false;
    status_ = Status::InFlight;
    out = last_;
    return true;
  }

  bool complete(Session session, uint64_t id, bool accepted) {
    if (!connected_ || !(session == session_) || id != last_.id ||
        status_ != Status::InFlight) return false;
    status_ = accepted ? Status::Accepted : Status::Rejected;
    return true;
  }

  Status result(Session session, uint64_t id) const {
    if (!connected_ || !(session == session_)) return Status::WrongSession;
    if (!id || id != last_.id) return Status::Unknown;
    return status_;
  }

private:
  Status validate(const Command& cmd, const Context& ctx, uint64_t nowUs) const {
    if (!(cmd.session == session_) || !(ctx.session == session_))
      return Status::WrongSession;
    if (cmd.expiresAtUs <= nowUs) return Status::Expired;
    if (cmd.expiresAtUs - nowUs > MaxFutureUs) return Status::Invalid;
    if (cmd.authorityGeneration != ctx.authorityGeneration ||
        cmd.lifecycleRevision != ctx.lifecycleRevision ||
        cmd.raceId != ctx.raceId || cmd.proposalRevision != ctx.proposalRevision)
      return Status::Stale;
    if (!ctx.tasterAllowed) return Status::Forbidden;
    if (cmd.action == Action::Setup) {
      if ((cmd.mode != Mode::Laps && cmd.mode != Mode::Timed) ||
          cmd.value < 1 || cmd.value > 999 || cmd.confirmed) return Status::Invalid;
      return ctx.lifecycle == Lifecycle::Ready ? Status::Queued : Status::Forbidden;
    }
    if (cmd.mode != Mode::None || cmd.value != 0 ||
        (cmd.action != Action::Abandon && cmd.confirmed)) return Status::Invalid;
    switch (cmd.action) {
      case Action::Start:
        return ctx.lifecycle == Lifecycle::Ready ? Status::Queued : Status::Forbidden;
      case Action::Pause:
        return ctx.lifecycle == Lifecycle::Racing ? Status::Queued : Status::Forbidden;
      case Action::Resume:
        return ctx.lifecycle == Lifecycle::Paused ? Status::Queued : Status::Forbidden;
      case Action::Abandon:
        if (!cmd.confirmed) return Status::Invalid;
        return ctx.lifecycle == Lifecycle::Paused ? Status::Queued : Status::Forbidden;
      case Action::RaceAgain:
        return ctx.lifecycle == Lifecycle::Finished ? Status::Queued : Status::Forbidden;
      default: return Status::Invalid;
    }
  }

  Session session_{};
  Command last_{};
  uint64_t highWater_ = 0;
  Status status_ = Status::Unknown;
  bool connected_ = false;
};

} // namespace pp::taster_link
