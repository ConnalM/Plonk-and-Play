#pragma once
#include <mutex>
#include "control_lease.h"
#include "core.h"

namespace pp {
// A coherent, committed view of the HTTP-owned lease. Race Control and
// Presentation may read it from other tasks. Never hold this lock over
// persistence, socket operations, message publication or race processing.
class PublishedControlAuthority final {
public:
  void publish(const ControlLease::Snapshot& committed) {
    std::lock_guard<std::mutex> guard(lock_);
    if(state_.owner!=committed.owner || state_.connectedOwner!=committed.connectedOwner)
      advanceGeneration();
    state_ = committed;
  }
  ControlLease::Snapshot read() const {
    std::lock_guard<std::mutex> guard(lock_);
    auto result = state_;
    if (!transportAvailable_) result.connectedOwner = 0;
    return result;
  }
  void suspendTransport() {
    std::lock_guard<std::mutex> guard(lock_);
    if(transportAvailable_) advanceGeneration();
    transportAvailable_ = false;
  }
  void resumeTransport() {
    std::lock_guard<std::mutex> guard(lock_);
    if(!transportAvailable_) advanceGeneration();
    transportAvailable_ = true;
  }
  bool hasController() const {
    const auto s = read();
    return s.owner != 0 && s.connectedOwner == s.owner;
  }
  bool authorizes(uint64_t fingerprint) const {
    const auto s = read();
    return fingerprint != 0 && s.owner == fingerprint &&
           s.connectedOwner == fingerprint;
  }
  StandaloneAuthorityStamp standaloneStamp() const {
    std::lock_guard<std::mutex> guard(lock_);
    return {generation_,generation_!=0 && !effectiveController()};
  }
  bool claimStandalone(uint64_t expected) const {
    std::lock_guard<std::mutex> guard(lock_);
    return expected!=0 && expected==generation_ && !effectiveController();
  }
private:
  bool effectiveController() const {
    return transportAvailable_ && state_.owner!=0 && state_.connectedOwner==state_.owner;
  }
  void advanceGeneration() {
    // Exhaustion permanently disables remote claims; never wrap to a valid stamp.
    if(generation_) generation_=generation_==UINT64_MAX?0:generation_+1;
  }
  mutable std::mutex lock_;
  ControlLease::Snapshot state_{};
  bool transportAvailable_ = true;
  uint64_t generation_ = 1;
};
}  // namespace pp
