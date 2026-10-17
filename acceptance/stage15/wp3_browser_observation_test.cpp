#include "pp/diagnostic_browser.h"

#include <cstring>

using namespace pp::diagnostic;

int main() {
  const char* body =
      "{\"kind\":\"state_received\",\"lifecycle\":\"RACING\","
      "\"sessionMode\":\"LAP_RACE\",\"operation\":\"START\","
      "\"phase\":\"GO\"}";
  char value[32]{};
  if (!jsonTextField(body, "\"kind\"", value, sizeof(value)) ||
      std::strcmp(value, "state_received") != 0) return 1;
  if (!jsonTextField(body, "\"lifecycle\"", value, sizeof(value)) ||
      std::strcmp(value, "RACING") != 0) return 2;
  EventKind event = EventKind::ResourceHealth;
  if (!jsonTextField(body, "\"kind\"", value, sizeof(value))) return 4;
  if (!browserStateObservationKind(value, event) ||
      event != EventKind::StateReceived) return 5;
  if (browserStateObservationKind("unknown", event)) return 6;

  // Unknown Browser observations are ignored by the production route rather
  // than being misclassified as ResourceHealth records.
  if (browserStateObservationKind("resource_health", event)) return 7;
  return 0;
}
