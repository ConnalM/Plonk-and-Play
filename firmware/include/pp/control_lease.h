#pragma once

#include <stdint.h>

namespace pp {

// The control lease is the P&P-owned association between a Browser identity
// and the single Master/Race Director/controlling-SMUG authority.  It contains
// no product or race state.
enum class ControlLeaseResult : uint8_t {
  Accepted,
  AlreadyOwner,
  Pending,
  AlreadyPending,
  Refused,
  NoPending,
  NotOwner,
};

class ControlLease final {
public:
  struct Snapshot {
    uint64_t owner = 0;
    uint64_t connectedOwner = 0;
    uint64_t requester = 0;
    uint64_t lastRequester = 0;
    uint32_t requestId = 0;
    uint32_t lastRequestId = 0;
    ControlLeaseResult lastResult = ControlLeaseResult::NoPending;
  };

  void reset() { state_ = Snapshot{}; nextRequestId_ = 0; }
  bool restore(uint64_t owner) {
    if (!owner) return false;
    state_.owner = owner;
    state_.connectedOwner = 0;
    return true;
  }
  bool hasOwner() const { return state_.owner != 0; }
  uint64_t owner() const { return state_.owner; }
  bool isOwner(uint64_t client) const { return client != 0 && state_.owner == client; }
  bool ownerConnected() const { return state_.owner != 0 && state_.connectedOwner == state_.owner; }
  void connected(uint64_t client) {
    if (isOwner(client)) state_.connectedOwner = client;
  }
  void disconnected(uint64_t client) {
    if (state_.connectedOwner == client) state_.connectedOwner = 0;
  }

  ControlLeaseResult request(uint64_t requester, uint32_t& requestId) {
    requestId = 0;
    if (!requester) return ControlLeaseResult::NotOwner;
    // A pending requester may deliberately complete acquisition after the
    // current owner has genuinely disconnected.  This is not an inactivity
    // timeout: the connection boundary has already invalidated the owner.
    if (state_.requester == requester && !ownerConnected()) {
      state_.owner = requester;
      state_.connectedOwner = 0;
      state_.requester = 0;
      state_.requestId = 0;
      state_.lastRequestId = ++nextRequestId_;
      if (!state_.lastRequestId) state_.lastRequestId = ++nextRequestId_;
      state_.lastResult = ControlLeaseResult::Accepted;
      requestId = state_.lastRequestId;
      return ControlLeaseResult::Accepted;
    }
    if (state_.requester != 0) {
      requestId = state_.requestId;
      return ControlLeaseResult::AlreadyPending;
    }
    if (!hasOwner() || !ownerConnected()) {
      state_.owner = requester;
      state_.connectedOwner = 0;
      state_.requester = 0;
      state_.requestId = 0;
      state_.lastRequestId = ++nextRequestId_;
      state_.lastResult = ControlLeaseResult::Accepted;
      requestId = state_.lastRequestId;
      return ControlLeaseResult::Accepted;
    }
    if (isOwner(requester)) return ControlLeaseResult::AlreadyOwner;
    state_.requester = requester;
    state_.lastRequester = requester;
    state_.requestId = ++nextRequestId_;
    if (!state_.requestId) state_.requestId = ++nextRequestId_;
    state_.lastRequestId = state_.requestId;
    state_.lastResult = ControlLeaseResult::Pending;
    requestId = state_.requestId;
    return ControlLeaseResult::Pending;
  }

  ControlLeaseResult respond(uint64_t owner, bool allow, uint32_t& requestId) {
    requestId = state_.requestId;
    if (!isOwner(owner)) return ControlLeaseResult::NotOwner;
    if (!state_.requester) return ControlLeaseResult::NoPending;
    const uint64_t requester = state_.requester;
    if (allow) {
      state_.owner = requester;
      state_.connectedOwner = 0;
      state_.requester = 0;
      state_.requestId = 0;
      state_.lastResult = ControlLeaseResult::Accepted;
      return ControlLeaseResult::Accepted;
    }
    state_.requester = 0;
    state_.requestId = 0;
    state_.lastResult = ControlLeaseResult::Refused;
    return ControlLeaseResult::Refused;
  }

  bool pending() const { return state_.requester != 0; }
  uint64_t requester() const { return state_.requester; }
  uint64_t lastRequester() const { return state_.lastRequester; }
  uint32_t requestId() const { return state_.requestId; }
  uint32_t lastRequestId() const { return state_.lastRequestId; }
  ControlLeaseResult lastResult() const { return state_.lastResult; }
  Snapshot snapshot() const { return state_; }
  void restoreSnapshot(const Snapshot& snapshot) { state_ = snapshot; }

private:
  Snapshot state_{};
  uint32_t nextRequestId_ = 0;
};

}  // namespace pp
