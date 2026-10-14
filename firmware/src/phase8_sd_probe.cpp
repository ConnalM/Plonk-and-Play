#if defined(PP_STAGE15_PHASE8_SD)

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>
#include <esp_spi_flash.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_idf_version.h>
#include <SD.h>

#include "pp/diagnostic_sd.h"
#include "pp/diagnostic_memory.h"

namespace {
pp::diagnostic::ControllerEpoch epoch{};

// Reused verbatim from the previously working ESP32-8048S070C display probe
// (Display3 outside this repository). These are the documented RGB-panel
// signals for the board; the only control output is the tested GPIO2
// backlight. This probe does not assume touch or other optional peripherals.
constexpr int kTftDe = 41;
constexpr int kTftVsync = 40;
constexpr int kTftHsync = 39;
constexpr int kTftPclk = 42;
constexpr int kTftR0 = 14;
constexpr int kTftR1 = 21;
constexpr int kTftR2 = 47;
constexpr int kTftR3 = 48;
constexpr int kTftR4 = 45;
constexpr int kTftG0 = 9;
constexpr int kTftG1 = 46;
constexpr int kTftG2 = 3;
constexpr int kTftG3 = 8;
constexpr int kTftG4 = 16;
constexpr int kTftG5 = 1;
constexpr int kTftB0 = 15;
constexpr int kTftB1 = 7;
constexpr int kTftB2 = 6;
constexpr int kTftB3 = 5;
constexpr int kTftB4 = 4;
constexpr int kBacklight = 2;
constexpr int kDisplayWidth = 800;
constexpr int kDisplayHeight = 480;

Arduino_ESP32RGBPanel* displayBus = nullptr;
Arduino_RGB_Display* display = nullptr;

void printMemory(const char* label) {
  Serial.printf("[PHASE8] %s heap_free=%u heap_min=%u heap_largest=%u psram_total=%u psram_free=%u psram_largest=%u flash=%u\n",
                label, ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), ESP.getPsramSize(),
                ESP.getFreePsram(), heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
                ESP.getFlashChipSize());
}

const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXTERNAL";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "OTHER";
  }
}

void printChip() {
  esp_chip_info_t info{};
  esp_chip_info(&info);
  Serial.printf("[PHASE8] chip model=%s revision=%d cores=%u features=0x%lx sdk=%s idf=%s\n",
                ESP.getChipModel(), ESP.getChipRevision(), unsigned(info.cores),
                static_cast<unsigned long>(info.features), ESP.getSdkVersion(), esp_get_idf_version());
  Serial.printf("[PHASE8] flash size=%u mode=%u speed_hz=%u psram=%u\n",
                ESP.getFlashChipSize(), unsigned(ESP.getFlashChipMode()), ESP.getFlashChipSpeed(),
                ESP.getPsramSize());
  Serial.printf("[PHASE8] reset reason=%s(%u)\n", resetReasonName(esp_reset_reason()), unsigned(esp_reset_reason()));
}

bool initDisplay() {
  Serial.printf("[PHASE8] display init begin width=%d height=%d backlight_gpio=%d\n",
                kDisplayWidth, kDisplayHeight, kBacklight);
  displayBus = new Arduino_ESP32RGBPanel(
      kTftDe, kTftVsync, kTftHsync, kTftPclk,
      kTftR0, kTftR1, kTftR2, kTftR3, kTftR4,
      kTftG0, kTftG1, kTftG2, kTftG3, kTftG4, kTftG5,
      kTftB0, kTftB1, kTftB2, kTftB3, kTftB4,
      0, 210, 30, 16, 0, 22, 13, 10, 1, 16000000);
  display = new Arduino_RGB_Display(kDisplayWidth, kDisplayHeight, displayBus, 0, true, nullptr, kBacklight);
  if (display == nullptr || !display->begin()) {
    Serial.println("[PHASE8] display init FAILED");
    return false;
  }
  display->setRotation(0);
  display->fillScreen(0x0000);
  display->setTextColor(0xFFFF);
  display->setTextSize(3);
  display->setCursor(40, 180);
  display->println("P&P PHASE 8B");
  display->setTextSize(2);
  display->setCursor(40, 230);
  display->println("ESP32-8048S070 PROBE");
  display->setCursor(40, 265);
  display->println("DISPLAY OK");
  Serial.println("[PHASE8] display init OK");
  return true;
}

bool runExplicitSdProbe() {
  char path[48]{};
  snprintf(path, sizeof(path), "/pp8-probe-%llu.txt", static_cast<unsigned long long>(epoch.value));
  const char expected[] = "P&P PHASE8 SD PROBE";
  char actual[sizeof(expected)]{};

  Serial.printf("[PHASE8] sd file open BEFORE path=%s\n", path);
  File output = SD.open(path, FILE_WRITE);
  Serial.printf("[PHASE8] sd file open AFTER ok=%u\n", output ? 1U : 0U);
  size_t written = 0;
  if (output) {
    Serial.println("[PHASE8] sd header write BEFORE");
    written = output.write(reinterpret_cast<const uint8_t*>(expected), sizeof(expected) - 1U);
    Serial.printf("[PHASE8] sd header write AFTER bytes=%u\n", static_cast<unsigned>(written));
    Serial.println("[PHASE8] sd flush BEFORE");
    output.flush();
    Serial.println("[PHASE8] sd flush AFTER");
    Serial.println("[PHASE8] sd file close BEFORE");
    output.close();
    Serial.println("[PHASE8] sd file close AFTER");
  }

  Serial.println("[PHASE8] sd read open BEFORE");
  File input = SD.open(path, FILE_READ);
  Serial.printf("[PHASE8] sd read open AFTER ok=%u\n", input ? 1U : 0U);
  size_t read = 0;
  if (input) {
    Serial.println("[PHASE8] sd read BEFORE");
    read = input.read(reinterpret_cast<uint8_t*>(actual), sizeof(expected) - 1U);
    Serial.printf("[PHASE8] sd read AFTER bytes=%u\n", static_cast<unsigned>(read));
    input.close();
  }
  const bool match = written == sizeof(expected) - 1U && read == sizeof(expected) - 1U &&
                     memcmp(actual, expected, sizeof(expected) - 1U) == 0;
  Serial.printf("[PHASE8] sd verify AFTER match=%u\n", match ? 1U : 0U);

  Serial.println("[PHASE8] sd delete BEFORE");
  const bool removed = SD.remove(path);
  Serial.printf("[PHASE8] sd delete AFTER ok=%u\n", removed ? 1U : 0U);

  Serial.println("[PHASE8] sd cardSize BEFORE");
  const uint64_t cardBytes = SD.cardSize();
  Serial.printf("[PHASE8] sd cardSize AFTER bytes=%llu\n", static_cast<unsigned long long>(cardBytes));
  Serial.println("[PHASE8] sd totalBytes BEFORE");
  const uint64_t totalBytes = SD.totalBytes();
  Serial.printf("[PHASE8] sd totalBytes AFTER bytes=%llu\n", static_cast<unsigned long long>(totalBytes));
  Serial.println("[PHASE8] sd usedBytes BEFORE");
  const uint64_t usedBytes = SD.usedBytes();
  Serial.printf("[PHASE8] sd usedBytes AFTER bytes=%llu free=%llu\n",
                static_cast<unsigned long long>(usedBytes),
                static_cast<unsigned long long>(totalBytes >= usedBytes ? totalBytes - usedBytes : 0));
  return match && removed;
}

}

void setup() {
  Serial.begin(115200);
  // The physical board's proven Display3 firmware waits briefly for the
  // serial monitor before emitting one-shot probe evidence. Keep this
  // bounded so a missing host cannot block peripheral probing forever.
  const uint32_t serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 3000U) delay(10);
  delay(300);
  Serial.println("[BUILD] STAGE15 PHASE8 ESP32-8048S070 SD PROBE");
  printChip();
  printMemory("boot");
  initDisplay();

  epoch.value = (static_cast<uint64_t>(micros()) << 16) ^ esp_random();
  if (!epoch.valid()) epoch.value = 1;
  Serial.printf("[PHASE8] sd mount BEFORE cs=%d mosi=%d sck=%d miso=%d\n",
                pp::diagnostic::Esp32SpiSdDiagnosticStorage::CsPin,
                pp::diagnostic::Esp32SpiSdDiagnosticStorage::MosiPin,
                pp::diagnostic::Esp32SpiSdDiagnosticStorage::SckPin,
                pp::diagnostic::Esp32SpiSdDiagnosticStorage::MisoPin);
  SPIClass probeSpi(FSPI);
  Serial.println("[PHASE8] spi begin BEFORE");
  probeSpi.begin(pp::diagnostic::Esp32SpiSdDiagnosticStorage::SckPin,
                 pp::diagnostic::Esp32SpiSdDiagnosticStorage::MisoPin,
                 pp::diagnostic::Esp32SpiSdDiagnosticStorage::MosiPin,
                 pp::diagnostic::Esp32SpiSdDiagnosticStorage::CsPin);
  Serial.println("[PHASE8] spi begin AFTER");
  Serial.println("[PHASE8] SD.begin BEFORE");
  const bool mounted = SD.begin(pp::diagnostic::Esp32SpiSdDiagnosticStorage::CsPin,
                                probeSpi, pp::diagnostic::Esp32SpiSdDiagnosticStorage::SpiFrequency);
  Serial.println("[PHASE8] SD.begin AFTER");
  const auto beginResult = mounted ? pp::diagnostic::StorageResult::Accepted
                                   : pp::diagnostic::StorageResult::Unavailable;
  Serial.printf("[PHASE8] sd mount AFTER result=%u mounted=%u\n", unsigned(beginResult), mounted ? 1U : 0U);
  if (!mounted) {
    Serial.printf("[PHASE8] sd mount FAILED cs=%d mosi=%d sck=%d miso=%d\n",
                  pp::diagnostic::Esp32SpiSdDiagnosticStorage::CsPin,
                  pp::diagnostic::Esp32SpiSdDiagnosticStorage::MosiPin,
                  pp::diagnostic::Esp32SpiSdDiagnosticStorage::SckPin,
                  pp::diagnostic::Esp32SpiSdDiagnosticStorage::MisoPin);
    Serial.println("[PHASE8] PROBE COMPLETE");
    return;
  }
  const bool fileOk = runExplicitSdProbe();
  Serial.printf("[PHASE8] SD fundamental test result=%u\n", fileOk ? 1U : 0U);
  printMemory("end");
  Serial.println("[PHASE8] PROBE COMPLETE");
}

void loop() {
  delay(1000);
}

#endif // PP_STAGE15_PHASE8_SD
