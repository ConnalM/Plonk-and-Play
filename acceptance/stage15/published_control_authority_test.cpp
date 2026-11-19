// Host-only focused contract for the publication boundary, not an HTTP test.
// g++ -std=c++17 -Wall -Wextra -Werror -pthread -Ifirmware/include //   acceptance/stage15/published_control_authority_test.cpp -o <scratch exe>
#include "pp/published_control_authority.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <thread>

int main() {
  using namespace pp;
  PublishedControlAuthority view;
  ControlLease live;
  assert(!view.hasController() && !view.authorizes(7));
  assert(live.restore(7));
  view.publish(live.snapshot());
  assert(view.read().owner == 7 && !view.hasController());
  live.connected(7);
  view.publish(live.snapshot());
  assert(view.authorizes(7) && !view.authorizes(8));

  // A pending takeover does not replace the committed controller.
  uint32_t id = 0;
  assert(live.request(8, id) == ControlLeaseResult::Pending);
  view.publish(live.snapshot());
  assert(view.authorizes(7));
  ControlLease next = live;
  assert(next.respond(7, true, id) == ControlLeaseResult::Accepted);
  next.connected(8);
  // Persistence has not succeeded yet: the candidate must be invisible.
  assert(view.authorizes(7) && !view.authorizes(8));
  assert(view.read().requester == 8);
  // Now model successful persistence and the single publication point.
  live = next;
  view.publish(live.snapshot());
  assert(view.authorizes(8) && !view.authorizes(7));

  // During HTTP shutdown, even a late connected publication cannot
  // reassert authority. Retained identity must survive shutdown.
  view.suspendTransport();
  view.publish(live.snapshot());
  assert(!view.hasController() && !view.authorizes(8));
  assert(view.read().owner == 8);
  live.disconnected(8);
  view.publish(live.snapshot());
  view.resumeTransport();
  assert(!view.hasController() && !view.authorizes(8));
  live.connected(8);
  view.publish(live.snapshot());
  assert(view.authorizes(8));

  // Alternate disjoint snapshots for cross-task readers. 7 is always
  // retained-but-disconnected or not the owner: no mixed read may grant 7.
  ControlLease::Snapshot a{};
  a.owner = 7;
  ControlLease::Snapshot b{};
  b.owner = b.connectedOwner = 8;
  view.publish(a);
  std::atomic<bool> done{false};
  std::atomic<bool> ready{false};
  std::thread reader([&] {
    ready = true;
    do {
      assert(!view.authorizes(7));
      const auto state = view.read();
      assert((state.owner == 7 && state.connectedOwner == 0) ||
             (state.owner == 8 && state.connectedOwner == 8));
    } while (!done.load());
  });
  while (!ready.load()) std::this_thread::yield();
  for (unsigned i = 0; i < 50000; ++i) {
    view.publish(b);
    view.publish(a);
  }
  done = true;
  reader.join();
  std::puts("PUBLISHED_CONTROL_AUTHORITY_PASS");
}
