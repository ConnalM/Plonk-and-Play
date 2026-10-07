const fs = require('fs');
const vm = require('vm');

const header = fs.readFileSync('firmware/include/pp/browser_interface.h', 'utf8');
const match = header.match(/function overtimeClock\(us\)\{([^}]*)\}/);
if (!match) throw new Error('missing production overtimeClock');
const context = {};
vm.runInNewContext(`globalThis.overtimeClock=function overtimeClock(us){${match[1]}}`, context);
const cases = [
  [0, '+00:00.0'],
  [95400000, '+01:35.4'],
  [59999000, '+00:59.9'],
  [60000000, '+01:00.0'],
];
for (const [microseconds, expected] of cases) {
  const actual = context.overtimeClock(microseconds);
  if (actual !== expected) throw new Error(`${microseconds} -> ${actual}, expected ${expected}`);
}
console.log('Stage 14C Browser presentation runtime PASS: overtime formatting remains valid MM:SS.s and carries minutes correctly');
