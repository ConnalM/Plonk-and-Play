#pragma once

#include <Arduino.h>
#include <Wire.h>

#define GT911_ADDR1 static_cast<uint8_t>(0x5D)
#define GT911_ADDR2 static_cast<uint8_t>(0x14)

#define ROTATION_LEFT static_cast<uint8_t>(0)
#define ROTATION_INVERTED static_cast<uint8_t>(1)
#define ROTATION_RIGHT static_cast<uint8_t>(2)
#define ROTATION_NORMAL static_cast<uint8_t>(3)

#define GT911_CONFIG_START static_cast<uint16_t>(0x8047)
#define GT911_X_OUTPUT_MAX_LOW static_cast<uint16_t>(0x8048)
#define GT911_X_OUTPUT_MAX_HIGH static_cast<uint16_t>(0x8049)
#define GT911_Y_OUTPUT_MAX_LOW static_cast<uint16_t>(0x804A)
#define GT911_Y_OUTPUT_MAX_HIGH static_cast<uint16_t>(0x804B)
#define GT911_CONFIG_CHKSUM static_cast<uint16_t>(0x80FF)
#define GT911_CONFIG_FRESH static_cast<uint16_t>(0x8100)
#define GT911_CONFIG_SIZE static_cast<uint8_t>(0xFF - 0x46)

#define GT911_POINT_INFO static_cast<uint16_t>(0x814E)
#define GT911_POINT_1 static_cast<uint16_t>(0x814F)

class TP_Point {
 public:
  TP_Point();
  TP_Point(uint8_t id, uint16_t x, uint16_t y, uint16_t size);

  bool operator==(TP_Point point);
  bool operator!=(TP_Point point);

  uint8_t id = 0;
  uint16_t x = 0;
  uint16_t y = 0;
  uint8_t size = 0;
};

// Project-owned, pinned GT911 adapter.  Its public surface intentionally keeps
// compatibility with TAMC_GT911 while exposing bounded-read diagnostics.
class TAMC_GT911 {
 public:
  TAMC_GT911(uint8_t sda, uint8_t scl, uint8_t intPin, uint8_t rstPin,
             uint16_t width, uint16_t height);

  bool begin(uint8_t address = GT911_ADDR1);
  void reset();
  void setRotation(uint8_t rotation);
  void setResolution(uint16_t width, uint16_t height);
  uint8_t getGesture();

  // true means the status register was read successfully.  Check
  // hasFreshSample() before consuming contact state: a valid 0x00 response
  // means no new GT911 report and leaves the previous contact state intact.
  bool read();

  uint8_t isLargeDetect = 0;
  uint8_t touches = 0;
  bool isTouched = false;
  TP_Point points[5]{};

  uint32_t pollCount() const { return pollCount_; }
  uint32_t readErrorCount() const { return readErrors_; }
  uint32_t invalidSampleCount() const { return invalidSamples_; }
  uint8_t lastPointInfo() const { return lastPointInfo_; }
  uint8_t lastReportedTouches() const { return lastReportedTouches_; }
  bool lastReadOk() const { return lastReadOk_; }
  bool hasFreshSample() const { return lastSampleFresh_; }

 private:
  bool readByteData(uint16_t reg, uint8_t& value);
  bool readBlockData(uint8_t* buffer, uint16_t reg, uint8_t size);
  bool writeByteData(uint16_t reg, uint8_t value);
  void calculateChecksum();
  bool reflashConfig();
  TP_Point readPoint(const uint8_t* data) const;
  bool clearPointInfo();
  void rejectSample(bool transportError);

  uint8_t rotation_ = ROTATION_NORMAL;
  uint8_t address_ = GT911_ADDR1;
  uint8_t pinSda_ = 0;
  uint8_t pinScl_ = 0;
  uint8_t pinInt_ = 0;
  uint8_t pinRst_ = 0;
  uint16_t width_ = 0;
  uint16_t height_ = 0;
  uint8_t configBuf_[GT911_CONFIG_SIZE]{};
  bool configValid_ = false;
  uint32_t pollCount_ = 0;
  uint32_t readErrors_ = 0;
  uint32_t invalidSamples_ = 0;
  uint8_t lastPointInfo_ = 0;
  uint8_t lastReportedTouches_ = 0;
  bool lastReadOk_ = false;
  bool lastSampleFresh_ = false;
};
