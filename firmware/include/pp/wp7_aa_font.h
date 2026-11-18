#pragma once

#include <Arduino.h>

// Compact renderer-owned font metadata. The bitmap stream stores two
// 4-bit coverage values per byte, high nibble first. Keeping this separate
// from Arduino_GFX's GFXfont prevents the monochrome drawChar() path from
// interpreting anti-aliased data as 1-bit pixels.
struct Wp7AaGlyph {
  uint32_t bitmapOffset;
  uint8_t width;
  uint8_t height;
  uint8_t xAdvance;
  int8_t xOffset;
  int8_t yOffset;
};

struct Wp7AaFont {
  const uint8_t* bitmap;
  const Wp7AaGlyph* glyph;
  uint16_t first;
  uint16_t last;
  uint8_t yAdvance;
};
