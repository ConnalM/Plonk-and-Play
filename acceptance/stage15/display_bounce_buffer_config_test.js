const fs = require('fs');

const path = 'firmware/include/pp/board_display_esp32s3.h';
const source = fs.readFileSync(path, 'utf8');

function requireMatch(label, pattern) {
  if (!pattern.test(source)) throw new Error(`${label}: missing expected production configuration`);
}

requireMatch('RGB timing and bounce argument',
  /0,\s*210,\s*30,\s*16,\s*0,\s*22,\s*13,\s*10,\s*1,\s*12000000,\s*false,\s*0,\s*0,\s*kBounceBufferSizePx/);
requireMatch('bounce pixel count', /kBounceBufferSizePx\s*=\s*20\s*\*\s*kDisplayWidth/);
requireMatch('per-buffer byte count', /kBounceBufferBytesEach\s*=\s*kBounceBufferSizePx\s*\*\s*sizeof\(uint16_t\)/);
requireMatch('two-buffer byte count', /kBounceBufferBytesTotal\s*=\s*2\s*\*\s*kBounceBufferBytesEach/);
requireMatch('single PSRAM framebuffer path',
  /kDisplayWidth,\s*kDisplayHeight,\s*displayBus_,\s*0,\s*true,\s*nullptr,\s*kBacklight/);
requireMatch('runtime bounce marker', /RGB bounce buffers configured: bounce_buffer_size_px=/);

if (!source.includes('static constexpr int kDisplayWidth = 800') ||
    !source.includes('static constexpr int kDisplayHeight = 480')) {
  throw new Error('display geometry changed');
}

console.log('ESP32-8048S070 production RGB bounce-buffer configuration: PASS');
