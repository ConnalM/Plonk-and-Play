const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.resolve(__dirname, '..', '..');
const embedded = fs.readFileSync(path.join(ROOT, 'firmware', 'include', 'pp', 'normal_browser_page.inc'), 'utf8');
const page = embedded.match(/R"PPHTML\(([\s\S]*)\)PPHTML";/)[1];
const script = page.match(/<script>([\s\S]*)<\/script>/)[1];
const expect = (value, message) => { if (!value) throw new Error(message); };

class Element {
  constructor(id) { this.id = id; this.textContent = ''; this.innerHTML = ''; this.disabled = false; this.onclick = null; this.classList = { toggle() {}, add() {}, remove() {} }; }
  set innerHTML(value) { this._innerHTML = String(value); }
  get innerHTML() { return this._innerHTML; }
}
const elements = {};
for (const id of ['logo', 'head', 'status', 'reason', 'app', 'overlay', 'notice', 'start', 'change']) elements[id] = new Element(id);
global.document = { elements, querySelector(selector) {
  const id = selector.replace(/^#/, '');
  if (!elements[id]) elements[id] = new Element(id);
  return elements[id];
} };
global.window = global;
global.$ = selector => document.querySelector(selector);
global.performance = { now: () => now };
global.sessionStorage = { getItem() { return null; }, setItem() {} };
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
  now += ms;
  found[1].callback();
};
const settle = async () => { for (let i = 0; i < 16; i++) await Promise.resolve(); };

let stateCalls = 0;
let proposalCalls = 0;
let contextCalls = 0;
const state = { lifecycle: 'READY', sessionMode: 'NONE', entries: [] };
const proposal = { mode: 'LAP_RACE', lapTarget: 2, durationMinutes: 1, finishBehaviour: 'IMMEDIATE', startable: true, proposalRevision: 1, entries: [] };
const response = value => ({ ok: true, status: 200, async json() { return JSON.parse(JSON.stringify(value)); } });
global.fetch = async pathName => {
  if (pathName === '/state') { ++stateCalls; return response(state); }
  if (pathName === '/proposal') { ++proposalCalls; return response(proposal); }
  if (pathName === '/context') { ++contextCalls; return response({ hasMaster: true, isMaster: true, role: 'Race Director' }); }
  if (pathName === '/noticeboard') return response({ revision: 1, proposalRevision: 1 });
  return response({});
};

const streams = [];
global.EventSource = class {
  static OPEN = 1; static CONNECTING = 0; static CLOSED = 2;
  constructor(url) { this.url = url; this.readyState = EventSource.CONNECTING; this.listeners = {}; streams.push(this); }
  addEventListener(name, callback) { this.listeners[name] = callback; }
  close() { this.readyState = EventSource.CLOSED; }
  open() { this.readyState = EventSource.OPEN; if (this.onopen) this.onopen(); }
  fail() { this.readyState = EventSource.CLOSED; if (this.onerror) this.onerror(); }
};

vm.runInThisContext(script, { filename: 'embedded-normal-browser.js' });

(async () => {
  await settle();
  const first = streams[0];
  expect(first, 'initial EventSource was not created');
  first.open();
  await settle();
  expect(connectionState === 'SYNCHRONISED', 'initial stream did not synchronise');
  const callsBeforeFailure = { state: stateCalls, proposal: proposalCalls, context: contextCalls };

  first.fail();
  expect(first.readyState === EventSource.CLOSED, 'failed EventSource was not retired');
  expect(connectionState === 'OFFLINE', 'failed EventSource did not mark Browser offline');
  expect(elements.start.disabled && elements.change.disabled, 'controls remained enabled while offline');
  expect([...timers.values()].filter(timer => timer.ms === 2000).length === 1, 'failure did not schedule exactly one bounded retry');
  first.fail();
  scheduleEventStreamRecovery('watchdog');
  expect([...timers.values()].filter(timer => timer.ms === 2000).length === 1, 'duplicate recovery signal created competing retries');
  await poll('sse');
  expect(stateCalls === callsBeforeFailure.state && proposalCalls === callsBeforeFailure.proposal && contextCalls === callsBeforeFailure.context,
    'offline SSE failure caused an acquisition storm');

  runTimer(2000);
  const replacement = streams[streams.length - 1];
  expect(replacement !== first && streams.length === 2, 'recovery did not create exactly one replacement attempt');
  replacement.open();
  await settle();
  expect(connectionState === 'SYNCHRONISED', 'replacement stream did not synchronise');
  expect(stateCalls > callsBeforeFailure.state && proposalCalls > callsBeforeFailure.proposal && contextCalls > callsBeforeFailure.context,
    'replacement did not perform fresh state/proposal/context acquisition');
  expect(elements.start.disabled === false, 'controls were not restored after fresh acquisition');
  console.log('Normal Browser SSE recovery PASS: one EventSource owner, bounded backoff, no offline polling storm, fresh reacquisition before controls');
})().catch(error => { console.error('Normal Browser SSE recovery FAIL:', error.stack || error.message); process.exitCode = 1; });
