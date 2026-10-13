#include "pp/control_lease.h"

using pp::ControlLease;
using pp::ControlLeaseResult;

int main() {
  ControlLease lease;
  bool activeRace = true; // Lease changes must not own or mutate product state.
  if (lease.hasOwner()) return 1;
  if (!lease.restore(11) || !lease.hasOwner()) return 2;
  lease.connected(11);
  if (!lease.ownerConnected() || !lease.isOwner(11)) return 3;

  uint32_t requestId = 0;
  if (lease.request(22, requestId) != ControlLeaseResult::Pending || !lease.pending()) return 4;
  const uint32_t first = requestId;
  if (lease.request(33, requestId) != ControlLeaseResult::AlreadyPending || requestId != first) return 5;
  if (lease.respond(22, true, requestId) != ControlLeaseResult::NotOwner) return 6;
  lease.connected(11); // A reload/reconnect of the current controller is valid.
  if (lease.respond(11, false, requestId) != ControlLeaseResult::Refused || lease.pending()) return 7;
  if (lease.lastResult() != ControlLeaseResult::Refused || lease.lastRequester() != 22) return 8;

  if (lease.request(22, requestId) != ControlLeaseResult::Pending) return 9;
  if (lease.respond(11, true, requestId) != ControlLeaseResult::Accepted) return 10;
  if (!lease.isOwner(22) || lease.isOwner(11) || lease.ownerConnected()) return 11;
  lease.connected(22);
  if (!lease.ownerConnected()) return 12;

  // A connected owner is never displaced by inactivity.  Only an explicit
  // disconnect evidence enables deliberate acquisition by another client.
  if (lease.request(33, requestId) != ControlLeaseResult::Pending) return 13;
  lease.disconnected(22);
  if (lease.request(44, requestId) != ControlLeaseResult::AlreadyPending) return 14;
  if (lease.request(33, requestId) != ControlLeaseResult::Accepted || !lease.isOwner(33)) return 15;
  if (!activeRace) return 16;
  return 0;
}
