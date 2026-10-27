const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.resolve(__dirname, '..', '..');
const embedded = fs.readFileSync(path.join(ROOT, 'firmware', 'include', 'pp', 'normal_browser_page.inc'), 'utf8');
const page = embedded.match(/R"PPHTML\(([\s\S]*)\)PPHTML";/)[1];
const script = page.match(/<script>([\s\S]*)<\/script>/)[1];

class Element {
  constructor(id) {
    this.id = id; this.textContent = ''; this.innerHTML = ''; this.value = '';
    this.disabled = false; this.hidden = false; this.onclick = null;
    this.classList = { toggle() {}, add() {}, remove() {} };
  }
  set innerHTML(value) {
    this._innerHTML = String(value);
    for (const found of this._innerHTML.matchAll(/id=["']([^"']+)["']/g)) {
      if (!document.elements[found[1]]) document.elements[found[1]] = new Element(found[1]);
    }
  }
  get innerHTML() { return this._innerHTML; }
  before() {}
}

const ids = ['logo', 'head', 'status', 'reason', 'app', 'overlay', 'notice', 'setupNav',
  'resultsNav', 'start', 'change', ...[...page.matchAll(/id=["']([^"']+)["']/g)].map(x => x[1])];
global.document = { elements: {}, querySelector(s) { return this.elements[s.replace(/^#/, '')] || null; }, createElement: id => new Element(id) };
for (const id of new Set(ids)) document.elements[id] = new Element(id);
global.window = global;
global.performance = { now: () => now };
global.sessionStorage = { getItem() { return null; }, setItem() {} };
global.$ = selector => document.querySelector(selector);
global.setInterval = () => 1;
global.clearInterval = () => {};

let now = 0;
let nextTimer = 1;
const timers = new Map();
global.setTimeout = (callback, ms) => { const id = nextTimer++; timers.set(id, { callback, ms }); return id; };
global.clearTimeout = id => timers.delete(id);
const runTimer = ms => {
  const found = [...timers.entries()].find(([, timer]) => timer.ms === ms);
  if (!found) throw new Error(`timer ${ms}ms was not scheduled`);
  timers.delete(found[0]);
  found[1].callback();
};

let contextMode = 'master';
let stateCalls = 0;
let proposalCalls = 0;
let contextCalls = 0;
const state = { lifecycle: 'READY', sessionMode: 'NONE', entries: [], practiceSummaryAvailable: false };
const proposal = { mode: 'LAP_RACE', lapTarget: 2, durationMinutes: 1, finishBehaviour: 'IMMEDIATE', startable: true, proposalRevision: 1, entries: [] };
function response(value, status = 200) { return { ok: status >= 200 && status < 300, status, async json() { return JSON.parse(JSON.stringify(value)); } }; }
global.fetch = async pathName => {
  if (pathName === '/state') { ++stateCalls; return response(state); }
  if (pathName === '/proposal') { ++proposalCalls; return response(proposal); }
  if (pathName === '/context') { ++contextCalls; return response(contextMode === 'master' ? { hasMaster: true, isMaster: true, role: 'Race Director' } : { hasMaster: true, isMaster: false, role: 'Spectator' }); }
  if (pathName === '/noticeboard') return response({ revision: 1, proposalRevision: 1 });
  return response({});
};

let streams = [];
global.EventSource = class {
  static OPEN = 1; static CONNECTING = 0; static CLOSED = 2;
  constructor(url) { this.url = url; this.readyState = EventSource.OPEN; this.listeners = {}; streams.push(this); }
  addEventListener(name, callback) { this.listeners[name] = callback; }
  close() { this.readyState = EventSource.CLOSED; }
  emit(name, event = {}) { if (this.listeners[name]) this.listeners[name](event); }
};

vm.runInThisContext(script, { filename: 'embedded-normal-browser.js' });

const expect = (value, message) => { if (!value) throw new Error(message); };
// Recovery deliberately fetches state, proposal, then context in separate
// awaits. Allow each response/json microtask to settle before asserting the
// post-recovery synchronisation state.
const settle = async () => { for (let i = 0; i < 32; i++) await Promise.resolve(); };

(async () => {
  await settle();
  const first = streams[0];
  expect(first, 'initial SSE stream was not created');
  first.onopen();
  await settle();
  expect(connectionState === 'SYNCHRONISED', 'initial SSE synchronisation failed');
  const callsAfterOpen = { state: stateCalls, proposal: proposalCalls, context: contextCalls };

  // A normal heartbeat refreshes liveness and must not create a replacement stream.
  now = 10000;
  first.emit('heartbeat', {});
  expect(streams.length === 1, 'normal heartbeat caused an unnecessary reconnect');

  // An apparently OPEN stream that receives no heartbeat beyond the bounded
  // tolerance is retired and replaced; the old stream cannot remain trusted.
  now = 40000;
  runTimer(25000);
  expect(connectionState === 'OFFLINE', 'stale SSE did not mark Browser unsynchronised');
  expect(first.readyState === EventSource.CLOSED, 'stale SSE was not retired');
  expect(document.elements.start.disabled && document.elements.change.disabled, 'controls remained enabled while unsynchronised');
  runTimer(2000);
  const replacement = streams[streams.length - 1];
  expect(replacement !== first && replacement.url === '/events', 'replacement SSE was not established');

  // Reconnection reacquires all authoritative resources before restoring the UI.
  contextMode = 'spectator';
  replacement.onopen();
  await settle();
  expect(connectionState === 'SYNCHRONISED', 'replacement SSE did not resynchronise');
  expect(stateCalls > callsAfterOpen.state && proposalCalls > callsAfterOpen.proposal && contextCalls > callsAfterOpen.context,
    'reconnection did not reacquire state, proposal and context');
  const recoveredIsMaster = vm.runInThisContext('C.isMaster');
  expect(recoveredIsMaster === false, 'authority was not derived from fresh /context');
  expect(!document.elements.start || document.elements.makeDirector, 'spectator presentation did not restore authority-gated controls');
  console.log('Normal Browser SSE liveness PASS: heartbeat retained healthy stream, stale OPEN stream retired, fresh state/proposal/context restored before synchronisation');
})().catch(error => { console.error('Normal Browser SSE liveness FAIL:', error.stack || error.message); process.exitCode = 1; });
