#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>

namespace pp {

// A deliberately small, non-blocking-at-consumer hand-off for HTTP acceptance
// fixtures.  The HTTP task only copies a request into this queue.  The product
// main loop is the sole consumer and therefore the only task that may mutate
// race, input, UI, or simulated-source state.
enum class FixtureQueueResult : uint8_t {
  Unknown,
  Queued,
  Accepted,
  Rejected,
  Expired,
  Cancelled,
  Busy,
  Full,
};

template <typename Payload, size_t QueueCapacity = 8, size_t OutcomeCapacity = 16>
class FixtureCommandQueue final {
public:
  struct Submission {
    uint32_t requestId = 0;
    FixtureQueueResult result = FixtureQueueResult::Unknown;
  };
  struct Work {
    uint32_t requestId = 0;
    uint32_t deadlineMs = 0;
    Payload payload{};
  };
  enum class Take : uint8_t { None, Busy, Expired, Ready };

  Submission submit(const Payload& payload, uint32_t nowMs, uint32_t timeoutMs) {
    std::unique_lock<std::mutex> guard(lock_, std::try_to_lock);
    if (!guard.owns_lock()) return {0, FixtureQueueResult::Busy};
    if (count_ == QueueCapacity) return {0, FixtureQueueResult::Full};
    uint32_t requestId = ++nextRequestId_;
    if (!requestId) requestId = ++nextRequestId_;
    queue_[tail_] = {requestId, nowMs + timeoutMs, payload};
    tail_ = (tail_ + 1U) % QueueCapacity;
    ++count_;
    recordUnlocked(requestId, FixtureQueueResult::Queued);
    return {requestId, FixtureQueueResult::Queued};
  }

  // The main-loop caller never blocks behind an HTTP handler.  An expired
  // command is recorded and discarded, so it can never execute late.
  Take tryTake(uint32_t nowMs, Work& work) {
    std::unique_lock<std::mutex> guard(lock_, std::try_to_lock);
    if (!guard.owns_lock()) return Take::Busy;
    if (!count_) return Take::None;
    work = queue_[head_];
    head_ = (head_ + 1U) % QueueCapacity;
    --count_;
    if (expired(nowMs, work.deadlineMs)) {
      recordUnlocked(work.requestId, FixtureQueueResult::Expired);
      return Take::Expired;
    }
    return Take::Ready;
  }

  bool tryComplete(uint32_t requestId, FixtureQueueResult result) {
    std::unique_lock<std::mutex> guard(lock_, std::try_to_lock);
    if (!guard.owns_lock()) return false;
    recordUnlocked(requestId, result);
    return true;
  }

  // Called at the reset/session boundary.  Cancelled commands have an
  // observable terminal outcome and are never subsequently taken.
  bool tryCancelPending() {
    std::unique_lock<std::mutex> guard(lock_, std::try_to_lock);
    if (!guard.owns_lock()) return false;
    while (count_) {
      recordUnlocked(queue_[head_].requestId, FixtureQueueResult::Cancelled);
      head_ = (head_ + 1U) % QueueCapacity;
      --count_;
    }
    return true;
  }

  FixtureQueueResult result(uint32_t requestId) const {
    std::unique_lock<std::mutex> guard(lock_, std::try_to_lock);
    if (!guard.owns_lock()) return FixtureQueueResult::Busy;
    for (const Outcome& outcome : outcomes_) {
      if (outcome.requestId == requestId) return outcome.result;
    }
    return FixtureQueueResult::Unknown;
  }

private:
  struct Outcome {
    uint32_t requestId = 0;
    FixtureQueueResult result = FixtureQueueResult::Unknown;
  };
  static bool expired(uint32_t nowMs, uint32_t deadlineMs) {
    return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
  }
  void recordUnlocked(uint32_t requestId, FixtureQueueResult result) {
    for (Outcome& outcome : outcomes_) {
      if (outcome.requestId == requestId) {
        outcome.result = result;
        return;
      }
    }
    outcomes_[nextOutcome_++ % OutcomeCapacity] = {requestId, result};
  }

  mutable std::mutex lock_;
  Work queue_[QueueCapacity]{};
  Outcome outcomes_[OutcomeCapacity]{};
  size_t head_ = 0, tail_ = 0, count_ = 0, nextOutcome_ = 0;
  uint32_t nextRequestId_ = 0;
};

}  // namespace pp
