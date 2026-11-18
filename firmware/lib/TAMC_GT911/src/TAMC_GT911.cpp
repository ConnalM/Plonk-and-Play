#include "TAMC_GT911.h"
#include "gt911_sample.h"

TP_Point::TP_Point() = default;

TP_Point::TP_Point(uint8_t pointId, uint16_t pointX, uint16_t pointY,
                   uint16_t pointSize)
    : id(pointId), x(pointX), y(pointY), size(static_cast<uint8_t>(pointSize)) {}

bool TP_Point::operator==(TP_Point point) {
  return id == point.id && x == point.x && y == point.y && size == point.size;
}

bool TP_Point::operator!=(TP_Point point) { return !(*this == point); }

TAMC_GT911::TAMC_GT911(uint8_t sda, uint8_t scl, uint8_t intPin,
                       uint8_t rstPin, uint16_t width, uint16_t height)
    : pinSda_(sda),
      pinScl_(scl),
      pinInt_(intPin),
      pinRst_(rstPin),
      width_(width),
      height_(height) {}

bool TAMC_GT911::begin(uint8_t address) {
  address_ = address;
  Wire.begin(pinSda_, pinScl_);
  reset();
  return configValid_;
}

void TAMC_GT911::reset() {
  pinMode(pinInt_, OUTPUT);
  pinMode(pinRst_, OUTPUT);
  digitalWrite(pinInt_, LOW);
  digitalWrite(pinRst_, LOW);
  delay(10);
  digitalWrite(pinInt_, address_ == GT911_ADDR2 ? HIGH : LOW);
  delay(1);
  digitalWrite(pinRst_, HIGH);
  delay(5);
  digitalWrite(pinInt_, LOW);
  delay(50);
  pinMode(pinInt_, INPUT);
  delay(50);

  configValid_ = readBlockData(configBuf_, GT911_CONFIG_START,
                               GT911_CONFIG_SIZE);
  if (configValid_) setResolution(width_, height_);
}

void TAMC_GT911::setRotation(uint8_t rotation) { rotation_ = rotation; }

void TAMC_GT911::setResolution(uint16_t width, uint16_t height) {
  width_ = width;
  height_ = height;
  if (!configValid_) return;
  configBuf_[GT911_X_OUTPUT_MAX_LOW - GT911_CONFIG_START] = lowByte(width_);
  configBuf_[GT911_X_OUTPUT_MAX_HIGH - GT911_CONFIG_START] = highByte(width_);
  configBuf_[GT911_Y_OUTPUT_MAX_LOW - GT911_CONFIG_START] = lowByte(height_);
  configBuf_[GT911_Y_OUTPUT_MAX_HIGH - GT911_CONFIG_START] = highByte(height_);
  configValid_ = reflashConfig();
}

uint8_t TAMC_GT911::getGesture() { return 0; }

bool TAMC_GT911::read() {
  ++pollCount_;
  lastReadOk_ = false;
  lastSampleFresh_ = false;
  lastPointInfo_ = 0;
  lastReportedTouches_ = 0;

  uint8_t pointInfo = 0;
  if (!readByteData(GT911_POINT_INFO, pointInfo)) {
    ++readErrors_;
    return false;
  }

  lastPointInfo_ = pointInfo;
  isLargeDetect = (pointInfo >> 6U) & 1U;
  const pp::Gt911PointInfo decoded = pp::decodeGt911PointInfo(pointInfo);
  lastReportedTouches_ = decoded.reportedTouches;

  // GT911 supports at most five point records.  A larger count is malformed;
  // do not clamp-and-use a partial report because that could create a false
  // press or overwrite the fixed points[5] array.
  if (!decoded.valid) {
    ++invalidSamples_;
    if (!clearPointInfo()) ++readErrors_;
    return false;
  }

  // 0x00 is a transport-successful poll with no fresh GT911 report.  It is
  // neither a release nor a failure, so retain the last complete contact
  // state and do not acknowledge data that was never marked ready.
  if (!decoded.fresh) {
    lastReadOk_ = true;
    return true;
  }

  lastSampleFresh_ = true;
  touches = 0;
  isTouched = false;

  if (decoded.touched) {
    uint8_t data[7]{};
    for (uint8_t i = 0; i < decoded.reportedTouches; ++i) {
      if (!readBlockData(data, GT911_POINT_1 + static_cast<uint16_t>(i) * 8U,
                         sizeof(data))) {
        ++invalidSamples_;
        ++readErrors_;
        (void)clearPointInfo();
        return false;
      }
      points[i] = readPoint(data);
    }
  }

  touches = decoded.reportedTouches;
  isTouched = decoded.touched;
  if (!clearPointInfo()) {
    ++readErrors_;
    touches = 0;
    isTouched = false;
    return false;
  }
  lastReadOk_ = true;
  return true;
}

TP_Point TAMC_GT911::readPoint(const uint8_t* data) const {
  uint16_t x = static_cast<uint16_t>(data[1]) |
               (static_cast<uint16_t>(data[2]) << 8U);
  uint16_t y = static_cast<uint16_t>(data[3]) |
               (static_cast<uint16_t>(data[4]) << 8U);
  const uint16_t size = static_cast<uint16_t>(data[5]) |
                        (static_cast<uint16_t>(data[6]) << 8U);
  uint16_t temp = 0;
  switch (rotation_) {
    case ROTATION_NORMAL:
      x = width_ - x;
      y = height_ - y;
      break;
    case ROTATION_LEFT:
      temp = x;
      x = width_ - y;
      y = temp;
      break;
    case ROTATION_INVERTED:
      break;
    case ROTATION_RIGHT:
      temp = x;
      x = y;
      y = height_ - temp;
      break;
    default:
      break;
  }
  return TP_Point(data[0], x, y, size);
}

bool TAMC_GT911::readByteData(uint16_t reg, uint8_t& value) {
  Wire.beginTransmission(address_);
  Wire.write(highByte(reg));
  Wire.write(lowByte(reg));
  if (Wire.endTransmission() != 0U) return false;
  if (Wire.requestFrom(address_, static_cast<uint8_t>(1)) != 1U) return false;
  if (Wire.available() < 1) return false;
  const int received = Wire.read();
  if (received < 0) return false;
  value = static_cast<uint8_t>(received);
  return true;
}

bool TAMC_GT911::readBlockData(uint8_t* buffer, uint16_t reg, uint8_t size) {
  if (!buffer || !size) return false;
  // Arduino Wire defaults to a 128-byte RX buffer, while the GT911
  // configuration block is 185 bytes.  Keep every transfer bounded and
  // validate each response instead of accepting a silently truncated read.
  constexpr uint8_t kReadChunk = 32;
  uint8_t offset = 0;
  while (offset < size) {
    const uint8_t chunk = static_cast<uint8_t>(
        (size - offset) < kReadChunk ? (size - offset) : kReadChunk);
    Wire.beginTransmission(address_);
    Wire.write(highByte(static_cast<uint16_t>(reg + offset)));
    Wire.write(lowByte(static_cast<uint16_t>(reg + offset)));
    if (Wire.endTransmission() != 0U) return false;
    if (Wire.requestFrom(address_, chunk) != chunk) return false;
    if (Wire.available() < chunk) return false;
    for (uint8_t i = 0; i < chunk; ++i) {
      const int received = Wire.read();
      if (received < 0) return false;
      buffer[offset + i] = static_cast<uint8_t>(received);
    }
    offset = static_cast<uint8_t>(offset + chunk);
  }
  return true;
}

bool TAMC_GT911::writeByteData(uint16_t reg, uint8_t value) {
  Wire.beginTransmission(address_);
  Wire.write(highByte(reg));
  Wire.write(lowByte(reg));
  Wire.write(value);
  return Wire.endTransmission() == 0U;
}

void TAMC_GT911::calculateChecksum() {
  uint8_t checksum = 0;
  for (uint8_t i = 0; i < GT911_CONFIG_SIZE; ++i) checksum += configBuf_[i];
  configBuf_[GT911_CONFIG_CHKSUM - GT911_CONFIG_START] =
      static_cast<uint8_t>((~checksum) + 1U);
}

bool TAMC_GT911::reflashConfig() {
  calculateChecksum();
  if (!writeByteData(GT911_CONFIG_CHKSUM,
                     configBuf_[GT911_CONFIG_CHKSUM - GT911_CONFIG_START])) {
    return false;
  }
  return writeByteData(GT911_CONFIG_FRESH, 1);
}

bool TAMC_GT911::clearPointInfo() {
  return writeByteData(GT911_POINT_INFO, 0);
}
