#pragma once

// Phase 8B-P1 only: a thin SD-backed implementation of the existing product
// Store seam. Transactional payload/commit/index recovery remains Phase 8B-P2.

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
    // P1 deliberately uses the existing Store seam only. It does not claim
    // the P2 transactional commit/index protocol.
    SD.remove(target);
    File file = SD.open(target, FILE_WRITE);
    if (!file) return false;
    const size_t written = file.write(data, size);
    file.flush();
    file.close();
    return written == size;
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
