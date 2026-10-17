#pragma once

#include <stddef.h>
#include <string.h>

#include "diagnostic_contract.h"

namespace pp {
namespace diagnostic {

// Browser diagnostic observations are optional evidence.  Keep the small
// JSON field parser here so the production route and its host acceptance test
// exercise the same field/value boundary.
inline bool jsonTextField(const char* body, const char* key, char* out,
                          size_t capacity) {
  if (!body || !key || !out || capacity == 0) return false;
  out[0] = 0;
  const char* p = strstr(body, key);
  const char* colon = p ? strchr(p + strlen(key), ':') : nullptr;
  const char* start = colon ? strchr(colon, '"') : nullptr;
  if (!start) return false;
  ++start;
  const char* end = strchr(start, '"');
  if (!end) return false;
  const size_t length = size_t(end - start);
  if (length >= capacity) return false;
  memcpy(out, start, length);
  out[length] = 0;
  return true;
}

inline bool browserStateObservationKind(const char* kind, EventKind& event) {
  if (!kind) return false;
  if (strcmp(kind, "state_fetch_started") == 0) {
    event = EventKind::StateFetchStarted;
    return true;
  }
  if (strcmp(kind, "state_received") == 0) {
    event = EventKind::StateReceived;
    return true;
  }
  if (strcmp(kind, "state_applied") == 0) {
    event = EventKind::StateApplied;
    return true;
  }
  if (strcmp(kind, "rendered") == 0) {
    event = EventKind::BrowserRender;
    return true;
  }
  if (strcmp(kind, "reconstruction") == 0) {
    event = EventKind::Reconstruction;
    return true;
  }
  return false;
}

} // namespace diagnostic
} // namespace pp
