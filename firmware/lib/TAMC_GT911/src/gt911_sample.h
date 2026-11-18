#pragma once

#include <stdint.h>

namespace pp {

struct Gt911PointInfo {
  bool valid = false;
  bool bufferReady = false;
  bool fresh = false;
  bool touched = false;
  uint8_t reportedTouches = 0;
};

inline Gt911PointInfo decodeGt911PointInfo(uint8_t pointInfo) {
  Gt911PointInfo result{};
  result.bufferReady = (pointInfo & 0x80U) != 0;
  result.fresh = result.bufferReady;
  result.reportedTouches = pointInfo & 0x0FU;
  // The GT911 protocol has five point registers.  Treat any larger count, or
  // a nonzero count without the ready bit, as a malformed sample.
  if (result.reportedTouches > 5U ||
      (!result.bufferReady && result.reportedTouches != 0U)) {
    return result;
  }
  // A zero status byte is a valid "no new data" response, not a fresh
  // zero-contact report.  The caller must retain its prior contact state.
  result.valid = true;
  result.touched = result.bufferReady && result.reportedTouches != 0U;
  return result;
}

}  // namespace pp
