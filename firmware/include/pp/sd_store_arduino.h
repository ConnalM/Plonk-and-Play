#pragma once

// WP2A: a thin SD-backed implementation of the existing product Store seam.
// SlotHistoryStore owns the metadata commit/index boundary; each individual
// Store write is staged and verified before it is atomically published.

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

#include "core.h"

namespace pp {

class Esp32SpiSdStore final : public Store {
public:
  static constexpr int CsPin = 10;
  static constexpr int MosiPin = 11;
  static constexpr int SckPin = 12;
  static constexpr int MisoPin = 13;
  static constexpr uint32_t SpiFrequency = 20000000U;

  bool begin(const char* root = "/pp-p1") {
    root_ = root ? root : "/pp-p1";
    Serial.printf("[P8-P1] SD mount begin cs=%d mosi=%d sck=%d miso=%d\n",
                  CsPin, MosiPin, SckPin, MisoPin);
    spi_.begin(SckPin, MisoPin, MosiPin, CsPin);
    mounted_ = SD.begin(CsPin, spi_, SpiFrequency);
    if (!mounted_) {
      Serial.println("[P8-P1] SD mount FAILED");
      return false;
    }
    SD.mkdir(root_);
    Serial.printf("[P8-P1] SD mount OK card=%llu total=%llu used=%llu\n",
                  static_cast<unsigned long long>(SD.cardSize()),
                  static_cast<unsigned long long>(SD.totalBytes()),
                  static_cast<unsigned long long>(SD.usedBytes()));
    return true;
  }

  bool available() const override { return mounted_; }

  bool read(unsigned slot, uint8_t* data, size_t size) override {
    if (!mounted_ || !data) return false;
    File file = SD.open(path(slot), FILE_READ);
    if (!file) return false;
    const size_t actual = file.size();
    if (actual != size) {
      memset(data, 0, size);
      file.close();
      return true;
    }
    const size_t got = file.read(data, size);
    file.close();
    return got == size;
  }

  bool write(unsigned slot, const uint8_t* data, size_t size) override {
    if (!mounted_ || !data) return false;
    const String target = path(slot);
    String temporary = target;
    temporary += ".tmp";
    SD.remove(temporary);
    File file = SD.open(temporary, FILE_WRITE);
    if (!file) return false;
    const size_t written = file.write(data, size);
    file.flush();
    file.close();
    if (written != size) {
      SD.remove(temporary);
      return false;
    }

    // Read back in bounded chunks before publication. This keeps the Store
    // seam independent of the final SD streaming/recovery implementation and
    // prevents a short/corrupt payload from becoming visible at target.
    File verify = SD.open(temporary, FILE_READ);
    if (!verify || verify.size() != size) {
      if (verify) verify.close();
      SD.remove(temporary);
      return false;
    }
    uint8_t buffer[128];
    size_t offset = 0;
    bool matches = true;
    while (offset < size) {
      const size_t want = (size - offset) < sizeof(buffer) ? (size - offset) : sizeof(buffer);
      if (verify.read(buffer, want) != want || memcmp(buffer, data + offset, want) != 0) {
        matches = false;
        break;
      }
      offset += want;
    }
    verify.close();
    if (!matches) {
      SD.remove(temporary);
      return false;
    }

    // ESP-IDF VFS rename is the publication boundary and leaves the prior
    // target intact if the rename cannot be completed.
    if (!SD.rename(temporary, target)) {
      SD.remove(temporary);
      return false;
    }
    return true;
  }

private:
  String root_ = "/pp-p1";
  SPIClass spi_{FSPI};
  bool mounted_ = false;

  String path(unsigned slot) const {
    String value(root_);
    value += "/slot-";
    value += String(slot);
    value += ".bin";
    return value;
  }
};

} // namespace pp
