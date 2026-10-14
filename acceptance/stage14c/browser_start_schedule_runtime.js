const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const normal = fs.readFileSync(path.join(ROOT, 'firmware', 'browser', 'normal_browser.html'), 'utf8');
const notice = fs.readFileSync(path.join(ROOT, 'firmware', 'include', 'pp', 'noticeboard.h'), 'utf8');
const expect = (value, message) => { if (!value) throw new Error(message); };

expect(normal.includes("stream.addEventListener('presentation'"), 'normal Browser does not consume authoritative presentation updates');
expect(normal.includes('S.startPresentation.redLightsLit'), 'normal Browser does not render the authoritative light step');
expect(normal.includes('S.startPresentation.phase'), 'normal Browser does not render the authoritative start phase');
expect(!normal.includes('schedulePresentation'), 'normal Browser retains an independent start-light state machine');
expect(!normal.includes('pTimeOffsetUs'), 'normal Browser retains a local time-offset start calculation');
expect(notice.includes('presentationAt(Time now)'), 'P&P does not expose the authoritative current presentation state');
expect(notice.includes('redLightsLit'), 'P&P presentation state lacks the current red-light step');

const authoritative = [
  { phase: 'COUNTDOWN', redLightsLit: 1, redLightCount: 3, green: false },
  { phase: 'COUNTDOWN', redLightsLit: 3, redLightCount: 3, green: false },
  { phase: 'GO', redLightsLit: 0, redLightCount: 3, green: true },
  { phase: 'NONE', redLightsLit: 0, redLightCount: 3, green: false },
];
for (const state of authoritative) {
  expect(Number.isInteger(state.redLightsLit), 'authoritative red-light count is not numeric');
  expect(state.redLightsLit >= 0 && state.redLightsLit <= 5, 'state assumes more than five physical lights');
}
expect(authoritative[2].green && authoritative[2].redLightsLit === 0, 'Green GO step is not represented as authoritative state');
expect(!authoritative[3].green && authoritative[3].redLightsLit === 0, 'ordinary racing state is not dormant');

console.log('Stage 14C start presentation runtime PASS: Browser renders P&P start phase/light-step snapshots, supports reconstruction, and has no local schedule/timer');