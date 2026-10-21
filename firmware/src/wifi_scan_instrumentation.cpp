#include <Arduino.h>
#include <esp_err.h>
#include <esp_wifi.h>

// Temporary Phase 8B fault-finding instrumentation.  The linker wrapper
// observes the exact ESP-IDF result while delegating immediately to the real
// driver call, so scan behaviour is unchanged and no credential material is
// inspected or emitted.
#if defined(PP_WIFI_SCAN_INSTRUMENTATION)
extern "C" esp_err_t __real_esp_wifi_scan_start(const wifi_scan_config_t* config,
                                                 bool block);

extern "C" esp_err_t __wrap_esp_wifi_scan_start(const wifi_scan_config_t* config,
                                                 bool block) {
  const esp_err_t result = __real_esp_wifi_scan_start(config, block);
  Serial.printf("[WIFI] esp_wifi_scan_start result=%d name=%s block=%s channel=%u scan_type=%u\n",
                static_cast<int>(result), esp_err_to_name(result),
                block ? "true" : "false",
                config ? static_cast<unsigned>(config->channel) : 0U,
                config ? static_cast<unsigned>(config->scan_type) : 0U);
  return result;
}
#endif
