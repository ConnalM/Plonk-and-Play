#include <cassert>
#include <cstdint>
#include "pp/fixture_command_queue.h"

struct Command { uint8_t value = 0; };

int main() {
  using Queue = pp::FixtureCommandQueue<Command, 2, 4>;
  Queue queue;
  auto first = queue.submit({1}, 100, 50);
  auto second = queue.submit({2}, 100, 50);
  assert(first.result == pp::FixtureQueueResult::Queued);
  assert(second.result == pp::FixtureQueueResult::Queued);
  assert(queue.submit({3}, 100, 50).result == pp::FixtureQueueResult::Full);

  Queue::Work work{};
  assert(queue.tryTake(120, work) == Queue::Take::Ready);
  assert(work.requestId == first.requestId && work.payload.value == 1);
  assert(queue.tryComplete(work.requestId, pp::FixtureQueueResult::Accepted));
  assert(queue.result(first.requestId) == pp::FixtureQueueResult::Accepted);

  assert(queue.tryTake(151, work) == Queue::Take::Expired);
  assert(queue.result(second.requestId) == pp::FixtureQueueResult::Expired);

  auto reset = queue.submit({4}, 200, 50);
  auto cancelled = queue.submit({5}, 200, 50);
  assert(queue.tryTake(201, work) == Queue::Take::Ready);
  assert(work.requestId == reset.requestId);
  assert(queue.tryCancelPending());
  assert(queue.result(cancelled.requestId) == pp::FixtureQueueResult::Cancelled);
}
