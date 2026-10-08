const fs = require('fs');
const vm = require('vm');

const header = fs.readFileSync('firmware/include/pp/browser_interface.h', 'utf8');
function presentation(name) {
  const escaped = name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const match = header.match(new RegExp('static const char ' + escaped + '\\[\\] = \\s*R"HTML\\(([\\s\\S]*?)\\)HTML";'));
  if (!match) throw new Error('missing ' + name);
  return match[1].replace(/^<script>/, '').replace(/<\/script>$/, '');
}

class Element {
  constructor(id) { this.id = id; this.textContent = ''; this.disabled = false; this.value = ''; this.onclick = null; }
  before() {}
  set innerHTML(value) {
    this.html = value;
    for (const id of (value.matchAll(/id="([^"]+)"/g))) document.elements[id[1]] = new Element(id[1]);
  }
  get innerHTML() { return this.html || ''; }
}

global.document = {
  elements: {},
  querySelector(selector) { return this.elements[selector.replace(/^#/, '')] || null; },
  createElement() { return new Element(''); }
};
global.$ = selector => document.querySelector(selector);
for (const id of ['start', 'pause', 'raceAgain', 'target3', 'lane1', 'lane2', 'resetTest', 'bootstrap', 'request']) document.elements[id] = new Element(id);
global.window = global;
global.browserHasMaster = true;
global.nextCorrelation = 1;
global.lastState = { lifecycle: 'READY', sessionMode: 'LAP_RACE', durationMinutes: 0, finishBehaviour: 'STOP_AT_ZERO' };
global.polledState = global.lastState;
global.poll = async () => {
  // This is the production Browser's normal refresh/reconciliation boundary.
  global.lastState = global.polledState;
};
global.operation = path => {
  if (path === '/request/race-again') {
    global.polledState = { lifecycle: 'READY', sessionMode: 'ENDURANCE', durationMinutes: 1, finishBehaviour: 'IMMEDIATE' };
  }
  if (path === '/request/end-race/confirm') {
    // Model the production Browser boundary after the confirmed abandonment:
    // Race Control is READY again and the committed Endurance setup is the
    // retained next-session proposal.
    global.polledState = { lifecycle: 'READY', sessionMode: 'ENDURANCE', durationMinutes: 1, finishBehaviour: 'IMMEDIATE' };
  }
};
document.elements.raceAgain.onclick = () => global.operation('/request/race-again');
const starts = [];
global.fetch = async (path, options = {}) => {
  if (path === '/request/start') starts.push(JSON.parse(options.body));
  return { status: path === '/request/start' ? 202 : 200, ok: true, async json() { return path.includes('request-result') ? { result: 'ACCEPTED' } : {}; } };
};
global.get = async path => (await global.fetch(path)).json();

vm.runInThisContext(presentation('stage14bPresentation'), { filename: 'stage14bPresentation' });
vm.runInThisContext(presentation('stage14cPresentation'), { filename: 'stage14cPresentation' });

async function refresh() { await window.poll(); }
function requireValue(condition, message) { if (!condition) throw new Error(message); }

async function endurance() {
  const select = document.elements.enduranceSelect;
  const minutes = document.elements.enduranceMinutes;
  const finish = document.elements.enduranceFinish;
  minutes.value = '7';
  select.onclick();
  finish.value = '1'; finish.onchange();
  await refresh();
  requireValue(select.textContent === 'ENDURANCE SELECTED', 'Endurance proposal was cleared during READY refresh');
  requireValue(document.elements.practiceSelect.textContent === 'SELECT OPEN PRACTICE', 'Practice proposal was not cleared');
  document.elements.start.onclick();
  requireValue(starts.length === 1, 'Endurance START was not submitted');
  requireValue(starts[0].mode === 'ENDURANCE' && starts[0].durationMinutes === 7 && starts[0].finishPolicy === 1,
               'Endurance START payload did not preserve the proposal');

  // Reproduce the real post-RACE AGAIN path: authoritative READY is still
  // reconstructed from the completed Endurance definition, then the local
  // finish proposal is edited and must survive the next poll.
  global.polledState = { lifecycle: 'FINISHED', sessionMode: 'ENDURANCE', durationMinutes: 1, finishBehaviour: 'IMMEDIATE' };
  await refresh();
  document.elements.raceAgain.onclick();
  await refresh();
  finish.value = '1'; finish.onchange();
  await refresh();
  requireValue(finish.value === '1', 'Finish Current Lap proposal was overwritten after RACE AGAIN');
  document.elements.start.onclick();
  requireValue(starts.length === 2 && starts[1].mode === 'ENDURANCE' && starts[1].finishPolicy === 1,
               'RACE AGAIN Endurance START did not preserve Finish Current Lap');
}

async function practice() {
  // A fresh page has no selected mode; the same production scripts are then exercised.
  for (const id of ['practiceSelect', 'practiceResume', 'practiceEnd']) document.elements[id].textContent = id === 'practiceSelect' ? 'SELECT OPEN PRACTICE' : '';
  global.polledState = { lifecycle: 'READY', sessionMode: 'LAP_RACE', durationMinutes: 0, finishBehaviour: 'STOP_AT_ZERO' };
  document.elements.practiceSelect.onclick();
  await refresh();
  requireValue(document.elements.practiceSelect.textContent === 'OPEN PRACTICE SELECTED', 'Practice proposal was cleared during READY refresh');
  requireValue(document.elements.enduranceSelect.textContent === 'SELECT ENDURANCE', 'Endurance proposal was not cleared');
  document.elements.start.onclick();
  requireValue(starts.length === 3 && starts[2].mode === 'OPEN_PRACTICE', 'Practice START payload did not preserve the proposal');
}

async function postAbandonment() {
  global.polledState = { lifecycle: 'RACING', sessionMode: 'ENDURANCE', durationMinutes: 1, finishBehaviour: 'COMPLETE_CURRENT_LAP' };
  await refresh();
  await global.operation('/request/end-race/confirm');
  await refresh();
  requireValue(global.lastState.lifecycle === 'READY' && document.elements.enduranceSelect.textContent === 'ENDURANCE SELECTED',
               'END RACE did not reconstruct the retained Endurance proposal');
  document.elements.practiceSelect.onclick();
  await refresh();
  requireValue(document.elements.practiceSelect.textContent === 'OPEN PRACTICE SELECTED' &&
               document.elements.enduranceSelect.textContent === 'SELECT ENDURANCE',
               'Open Practice did not replace the retained Endurance proposal');
  document.elements.start.onclick();
  requireValue(starts.length === 4 && starts[3].mode === 'OPEN_PRACTICE',
               'post-abandonment Browser START did not submit the new mode');
}

(async () => {
  await endurance();
  await practice();
  await postAbandonment();
  console.log('Stage 14C Browser proposal/start runtime PASS: READY refresh and post-abandonment proposal replacement preserve production START payloads');
})().catch(error => { console.error('Stage 14C Browser proposal/start runtime FAIL:', error.message); process.exitCode = 1; });
