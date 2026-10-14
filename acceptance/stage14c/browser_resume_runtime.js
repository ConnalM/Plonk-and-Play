const fs = require('fs');
const vm = require('vm');

const inc = fs.readFileSync('firmware/include/pp/normal_browser_page.inc', 'utf8');
const pageMatch = inc.match(/R"PPHTML\(([\s\S]*)\)PPHTML";/);
if (!pageMatch) throw new Error('generated normal Browser payload missing');
const page = pageMatch[1];
const scriptMatch = page.match(/<script>([\s\S]*)<\/script>/);
if (!scriptMatch) throw new Error('normal Browser script missing');
const script = scriptMatch[1];
const raceStart = script.indexOf('function race(){');
const pausedStart = script.indexOf('function paused(){', raceStart);
if (raceStart < 0 || pausedStart < 0) throw new Error('race renderer boundaries missing');
const raceFn = script.slice(raceStart, pausedStart);

function render(state) {
  const A = { innerHTML: '', insertAdjacentHTML() {} };
  const context = {
    S: state,
    A,
    busy: false,
    sec: n => String(n ?? 0),
    clock: us => String(us),
    overtimeClock: us => String(us),
    pod: () => '',
    gap: () => '—',
    req: () => {},
    $: () => null,
  };
  vm.runInNewContext(`${raceFn}\nrace();`, context);
  return A.innerHTML;
}

if (!page.includes("phase==='RESUME_COUNTDOWN'")) throw new Error('Resume phase guard missing from generated Browser');
const base = {
  sessionMode: 'ENDURANCE', durationMinutes: 10, entries: [], finishBehaviour: 'STOP_AT_ZERO',
  finishDisplay: { active: false }, startPresentation: { phase: 'COUNTDOWN' }, lifecycle: 'STARTING',
  remainingDuration: 0, overtime: 0,
};
const initial = render({ ...base, startPresentation: { phase: 'COUNTDOWN' }, remainingDuration: 0 });
if (!initial.includes('<div class="clock">600000000</div>')) throw new Error('initial COUNTDOWN did not use configured duration');
const resumed = render({ ...base, startPresentation: { phase: 'RESUME_COUNTDOWN' }, remainingDuration: 23000000 });
if (!resumed.includes('<div class="clock">23000000</div>') || resumed.includes('<div class="clock">600000000</div>')) throw new Error('RESUME_COUNTDOWN did not use remainingDuration');
const racing = render({ ...base, lifecycle: 'RACING', startPresentation: { phase: 'GO' }, remainingDuration: 17000000 });
if (!racing.includes('<div class="clock">17000000</div>')) throw new Error('RACING did not use authoritative remainingDuration');
if (page.includes('id="endRace"')) throw new Error('stale active END RACE control remains in generated normal Browser');
if (!page.includes('id="abandon"') || !page.includes('id="endPractice"')) throw new Error('paused termination controls missing');
console.log('Stage 14C Browser resume runtime PASS: initial COUNTDOWN uses configured duration; RESUME_COUNTDOWN and RACING use authoritative remainingDuration; termination matrix present');
