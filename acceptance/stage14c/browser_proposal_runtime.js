const fs = require('fs');
const vm = require('vm');

const header = fs.readFileSync('firmware/include/pp/browser_interface.h', 'utf8');
function staticScript(name) {
  const escaped = name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const match = header.match(new RegExp('static const char ' + escaped + '\\[\\] = \\s*R"HTML\\(([\\s\\S]*?)\\)HTML";'));
  if (!match) throw new Error('missing ' + name);
  return match[1].replace(/^<script>/, '').replace(/<\/script>$/, '');
}
const html = header.match(/static const char html\[\] = R"HTML\(([\s\S]*?)\)HTML";/)[1];
const productionPageScript = html.match(/<script>([\s\S]*?)<\/script>/)[1];

class Element {
  constructor(id) { this.id = id; this.textContent = ''; this.disabled = false; this.hidden = false; this.value = ''; this._innerHTML = ''; this._onclick = null; this.onclickAssignments = 0; }
  set onclick(handler) { this._onclick = handler; this.onclickAssignments++; }
  get onclick() { return this._onclick; }
  dispatchEvent(event) { if (event && event.type === 'click' && this._onclick) return this._onclick(event); }
  before() {}
  set innerHTML(value) {
    this._innerHTML = value;
    for (const id of String(value).matchAll(/(?:id|name)=["']([^"']+)["']/g)) {
      if (!global.document.elements[id[1]]) global.document.elements[id[1]] = new Element(id[1]);
    }
  }
  get innerHTML() { return this._innerHTML; }
}

const ids = [
  'connection','role','human','request','state','fact','bootstrap','start','pause','honour','grid',
  'raceAgain','target3','lane1','lane2','resetTest','results','details','history','back','home',
  'endRace','restartRace','resultView'
];
global.document = { elements: {}, querySelector(selector) { return this.elements[selector.replace(/^#/, '')] || null; }, createElement() { return new Element(''); } };
for (const id of ids) document.elements[id] = new Element(id);
global.$ = selector => document.querySelector(selector);
global.window = global;
global.performance = { now: () => Date.now() };
const realSetTimeout = global.setTimeout;
global.setTimeout = (fn, _ms) => realSetTimeout(fn, 1);
global.setInterval = () => 1;
global.clearInterval = () => {};

let contextState = { role: 'Race Director', hasMaster: true };
let authoritativeState = { lifecycle: 'READY', sessionMode: 'NONE', durationMinutes: 0, finishBehaviour: 'IMMEDIATE', entries: [{ lane: 1, raceEntryId: 1, laps: 0 }, { lane: 2, raceEntryId: 2, laps: 0 }] };
let staleStates = [];
let revision = 1;
const starts = [];
function response(value, status = 200) { return { ok: status >= 200 && status < 300, status, async json() { return JSON.parse(JSON.stringify(value)); } }; }
function readyState() { return { lifecycle: 'READY', sessionMode: 'NONE', durationMinutes: 0, finishBehaviour: 'IMMEDIATE', entries: [{ lane: 1, raceEntryId: 1, laps: 0 }, { lane: 2, raceEntryId: 2, laps: 0 }] }; }
function activeState(mode, finishBehaviour = 'IMMEDIATE') { return { lifecycle: 'RACING', sessionMode: mode, durationMinutes: mode === 'ENDURANCE' ? 1 : 0, finishBehaviour, entries: [{ lane: 1, raceEntryId: 1, laps: 0 }, { lane: 2, raceEntryId: 2, laps: 0 }] }; }
function queueStale(state) { staleStates.push(state); }

global.fetch = async (path, options = {}) => {
  if (path === '/noticeboard') return response({ revision });
  if (path === '/context') return response(contextState);
  if (path === '/state') return response(staleStates.length ? staleStates.shift() : authoritativeState);
  if (path === '/fact') return response({ type: 'NONE', revision: 0 });
  if (path === '/request/end-race/confirm') { authoritativeState = readyState(); ++revision; return response({ submitted: true, correlationId: 1 }, 202); }
  if (path === '/request/race-again') { authoritativeState = { ...readyState(), sessionMode: 'ENDURANCE', durationMinutes: 1, finishBehaviour: 'IMMEDIATE' }; ++revision; return response({ submitted: true }, 202); }
  if (path === '/request/start') {
    const body = JSON.parse(options.body);
    starts.push(body);
    authoritativeState = activeState(body.mode, body.mode === 'ENDURANCE' && body.finishPolicy === 1 ? 'COMPLETE_CURRENT_LAP' : 'IMMEDIATE');
    ++revision;
    return response({ submitted: true, correlationId: body.correlationId }, 202);
  }
  if (path.startsWith('/request-result')) return response({ result: 'ACCEPTED', reason: 'NONE' });
  return response({}, 200);
};

// Execute the production page script first. The mode scripts then wrap this
// real poll() implementation exactly as they do in the served Browser page.
vm.runInThisContext(productionPageScript, { filename: 'browser-interface-html.js' });
vm.runInThisContext(staticScript('stage14bPresentation'), { filename: 'stage14bPresentation' });
vm.runInThisContext(staticScript('stage14cPresentation'), { filename: 'stage14cPresentation' });
document.elements.practiceSelect.textContent = 'SELECT OPEN PRACTICE';
document.elements.enduranceSelect.textContent = 'SELECT ENDURANCE';

async function poll() { await window.poll(); }
function requireValue(condition, message) { if (!condition) throw new Error(message); }
function requireExclusive(expected) {
  const practice = document.elements.practiceSelect.textContent === 'OPEN PRACTICE SELECTED';
  const endurance = document.elements.enduranceSelect.textContent === 'ENDURANCE SELECTED';
  requireValue(practice !== endurance, 'mode controls were not mutually exclusive');
  requireValue(expected === 'practice' ? practice : !practice, 'unexpected mode selection after reconciliation');
}

async function initialReadyAndEnduranceAbandonment() {
  await poll();
  requireValue(document.elements.practiceSelect.textContent === 'SELECT OPEN PRACTICE' && document.elements.enduranceSelect.textContent === 'SELECT ENDURANCE', 'clean READY did not start with neutral mode proposals');
  document.elements.enduranceSelect.dispatchEvent({ type: 'click' });
  await poll();
  requireExclusive('endurance');
  requireValue(document.elements.start.onclickAssignments === 1, 'START has more than one DOM owner');
  document.elements.start.dispatchEvent({ type: 'click' });
  await poll();
  requireValue(starts[0].mode === 'ENDURANCE' && authoritativeState.sessionMode === 'ENDURANCE', 'initial Endurance proposal did not start authoritatively');
  await global.fetch('/request/end-race/confirm', { method: 'POST', body: '{"correlationId":1}' });
  await poll();
  requireValue(authoritativeState.lifecycle === 'READY' && authoritativeState.sessionMode === 'NONE', 'Endurance abandonment did not return READY/NONE');
  requireExclusive('endurance');
}

async function staleEnduranceCannotOverwritePractice() {
  document.elements.practiceSelect.dispatchEvent({ type: 'click' });
  requireExclusive('practice');
  for (let i = 0; i < 40; i++) {
    queueStale(activeState('ENDURANCE', 'COMPLETE_CURRENT_LAP'));
    await poll();
    requireExclusive('practice');
    requireValue(global.pendingMode === 'OPEN_PRACTICE', 'repeated stale polling changed the pending mode');
  }
  const before = starts.length;
  document.elements.start.dispatchEvent({ type: 'click' });
  await poll();
  requireValue(starts.length === before + 1, 'one Browser START click did not submit exactly one request');
  requireValue(starts[before].mode === 'OPEN_PRACTICE', 'stale Endurance poll changed the Open Practice START payload');
  requireValue(authoritativeState.lifecycle === 'RACING' && authoritativeState.sessionMode === 'OPEN_PRACTICE' && authoritativeState.entries.length === 2, 'Open Practice START did not create the retained-entry authoritative session');
}

async function reverseDirectionAndRaceAgain() {
  await global.fetch('/request/end-race/confirm', { method: 'POST', body: '{"correlationId":2}' });
  await poll();
  document.elements.enduranceSelect.dispatchEvent({ type: 'click' });
  requireExclusive('endurance');
  queueStale(activeState('OPEN_PRACTICE'));
  await poll();
  requireExclusive('endurance');
  const before = starts.length;
  document.elements.start.dispatchEvent({ type: 'click' });
  await poll();
  requireValue(starts.length === before + 1, 'reverse START click did not submit exactly one request');
  requireValue(starts[before].mode === 'ENDURANCE' && authoritativeState.sessionMode === 'ENDURANCE', 'stale Open Practice poll changed the Endurance START payload');

  authoritativeState = { lifecycle: 'FINISHED', sessionMode: 'ENDURANCE', durationMinutes: 1, finishBehaviour: 'IMMEDIATE', entries: authoritativeState.entries };
  await poll();
  await global.fetch('/request/race-again', { method: 'POST', body: '{"correlationId":3}' });
  await poll();
  requireExclusive('endurance');

  authoritativeState = activeState('LAP_RACE');
  await global.fetch('/request/end-race/confirm', { method: 'POST', body: '{"correlationId":4}' });
  await poll();
  document.elements.practiceSelect.dispatchEvent({ type: 'click' });
  queueStale(activeState('LAP_RACE'));
  await poll();
  requireExclusive('practice');
}

(async () => {
  await new Promise(resolve => realSetTimeout(resolve, 5));
  await initialReadyAndEnduranceAbandonment();
  await staleEnduranceCannotOverwritePractice();
  await reverseDirectionAndRaceAgain();
  console.log('Stage 14C Browser proposal/start runtime PASS: production page polling preserves both mode proposals across stale active responses, abandonment and Race Again');
})().catch(error => { console.error('Stage 14C Browser proposal/start runtime FAIL:', error.stack || error.message); process.exitCode = 1; });
