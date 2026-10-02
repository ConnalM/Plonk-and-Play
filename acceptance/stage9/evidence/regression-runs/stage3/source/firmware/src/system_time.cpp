#include "pp/system_time.h"
#include <esp_timer.h>

uint64_t pp::systemTime() {
  return static_cast<uint64_t>(esp_timer_get_time());
}
