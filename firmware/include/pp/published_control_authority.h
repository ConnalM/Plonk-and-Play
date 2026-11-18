#pragma once
#include <mutex>
#include "control_lease.h"

namespace pp {
// A coherent, committed view of the HTTP-owned lease. Race Control and
// Presentation may read it from other tasks. Never hold this lock over
// persistence, socket operations, message publication or race processing.
class PublishedControlAuthority final {
public:
  void publish(const ControlLease::Snapshot& committed) {
    std::lock_guard<std::mutex> guard(lock_);
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
    transportAvailable_ = false;
  }
  void resumeTransport() {
    std::lock_guard<std::mutex> guard(lock_);
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
private:
  mutable std::mutex lock_;
  ControlLease::Snapshot state_{};
  bool transportAvailable_ = true;
};
}  // namespace pp
