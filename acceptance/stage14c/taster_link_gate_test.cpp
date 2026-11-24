#include "pp/taster_link_gate.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>

using namespace pp::taster_link;
int main() {
  CommandGate gate;
  constexpr uint64_t now = 1000000;
  Context c{{1, 2}, 1, 1, 0, 7, Lifecycle::Ready, true};
  auto cmd = [&](uint64_t id, Action a = Action::Start) {
    Command q{};
    q.session = c.session; q.id = id; q.action = a;
    q.authorityGeneration = c.authorityGeneration;
    q.lifecycleRevision = c.lifecycleRevision; q.raceId = c.raceId;
    q.proposalRevision = c.proposalRevision; q.expiresAtUs = now + 200000;
    return q;
  };
  Command out{};
  assert(gate.submit(cmd(1), c, now) == Status::WrongSession);
  assert(!gate.open({0,2}));
  assert(gate.open(c.session));
  assert(!gate.open(c.session));
  auto first = cmd(1);
  assert(gate.submit(first, c, now) == Status::Queued);
  assert(gate.submit(first, c, now) == Status::Queued);
  auto conflicting = first; conflicting.expiresAtUs++;
  assert(gate.submit(conflicting, c, now) == Status::Conflict);
  assert(gate.submit(cmd(2), c, now) == Status::Busy);
  assert(gate.take(c, now, out) && out == first);
  assert(!gate.take(c, now, out));
  assert(gate.result(c.session, 1) == Status::InFlight);
  assert(!gate.complete({1,9}, 1, true));
  assert(!gate.complete(c.session, 2, true));
  assert(gate.complete(c.session, 1, true));
  assert(gate.submit(first, c, now) == Status::Accepted);
  assert(!gate.take(c, now, out));
  assert(gate.submit(cmd(2), c, now) == Status::OldId);

  // Browser takes over and releases before a queued Taster command executes.
  auto delayed = cmd(3);
  assert(gate.submit(delayed, c, now) == Status::Queued);
  c.authorityGeneration += 2;
  assert(!gate.take(c, now, out));
  assert(gate.result(c.session, 3) == Status::Stale);
  assert(gate.submit(delayed, c, now) == Status::Stale);
  c.tasterAllowed = false;
  assert(gate.submit(cmd(4), c, now) == Status::Forbidden);
  c.tasterAllowed = true;

  auto expires = cmd(5);
  assert(gate.submit(expires, c, now) == Status::Queued);
  assert(!gate.take(c, expires.expiresAtUs, out));
  assert(gate.result(c.session, 5) == Status::Expired);
  auto tooFar = cmd(6); tooFar.expiresAtUs = now + CommandGate::MaxFutureUs + 1;
  assert(gate.submit(tooFar, c, now) == Status::Invalid);
  auto wrongRevision = cmd(7); wrongRevision.proposalRevision--;
  assert(gate.submit(wrongRevision, c, now) == Status::Stale);

  auto setup = cmd(8, Action::Setup); setup.mode = Mode::Laps; setup.value = 999;
  assert(gate.submit(setup, c, now) == Status::Queued);
  assert(gate.take(c, now, out));
  assert(gate.complete(c.session, 8, false));
  assert(gate.submit(setup, c, now) == Status::Rejected);
  setup.id = 9; setup.value = 1000;
  assert(gate.submit(setup, c, now) == Status::Invalid);
  setup.id = 10; setup.mode = Mode::Timed; setup.value = 1;
  assert(gate.submit(setup, c, now) == Status::Queued);
  ++c.proposalRevision;
  assert(!gate.take(c, now, out));
  assert(gate.result(c.session, 10) == Status::Stale);

  // Link drops with a queued command: never deliver it on the next session.
  auto queued = cmd(11);
  assert(gate.submit(queued, c, now) == Status::Queued);
  gate.disconnect();
  assert(!gate.take(c, now, out));
  c.session.link++;
  assert(gate.open(c.session));
  assert(gate.submit(queued, c, now) == Status::WrongSession);
  auto inflight = cmd(1);
  assert(gate.submit(inflight, c, now) == Status::Queued);
  assert(gate.take(c, now, out));
  // Lost result after dispatch is unknown; caller must reconcile snapshot.
  gate.disconnect();
  assert(!gate.complete(c.session, 1, true));
  c.session.link++;
  assert(gate.open(c.session));
  assert(gate.submit(inflight, c, now) == Status::WrongSession);

  c.lifecycle = Lifecycle::Racing; c.raceId = 42; ++c.lifecycleRevision;
  auto pause = cmd(1, Action::Pause);
  assert(gate.submit(pause, c, now) == Status::Queued);
  assert(gate.take(c, now, out)); assert(gate.complete(c.session, 1, true));
  c.lifecycle = Lifecycle::Paused; ++c.lifecycleRevision;
  assert(gate.submit(cmd(2, Action::Abandon), c, now) == Status::Invalid);
  auto abandon = cmd(3, Action::Abandon); abandon.confirmed = true;
  assert(gate.submit(abandon, c, now) == Status::Queued);
  // Core may still reject (e.g. pause settlement pending).
  assert(gate.take(c, now, out)); assert(gate.complete(c.session, 3, false));
  assert(gate.submit(cmd(4, Action::Resume), c, now) == Status::Queued);
  assert(gate.take(c, now, out)); assert(gate.complete(c.session, 4, true));
  c.lifecycle = Lifecycle::Finished; ++c.lifecycleRevision;
  assert(gate.submit(cmd(5, Action::RaceAgain), c, now) == Status::Queued);
  assert(gate.take(c, now, out)); assert(gate.complete(c.session, 5, true));
  auto invalid = cmd(6); invalid.action = static_cast<Action>(255);
  assert(gate.submit(invalid, c, now) == Status::Invalid);

  // IDs do not wrap. A different authenticated link is needed to restart.
  c.lifecycle = Lifecycle::Ready; ++c.lifecycleRevision; c.raceId = 0;
  auto max = cmd(std::numeric_limits<uint64_t>::max());
  assert(gate.submit(max, c, now) == Status::Queued);
  assert(gate.take(c, now, out)); assert(gate.complete(c.session, max.id, true));
  assert(gate.submit(cmd(1), c, now) == Status::OldId);
  c.session = {2,1};
  assert(gate.open(c.session));
  assert(gate.submit(max, c, now) == Status::WrongSession);
  assert(gate.submit(cmd(1), c, now) == Status::Queued);
  std::cout << "S3a command gate host prototype PASS\n";
}
