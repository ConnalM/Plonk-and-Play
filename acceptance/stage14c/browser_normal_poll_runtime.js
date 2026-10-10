const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.resolve(__dirname, '..', '..');
const embeddedPath = path.join(ROOT, 'firmware', 'include', 'pp', 'normal_browser_page.inc');
if (!fs.existsSync(embeddedPath)) {
  throw new Error('embedded normal Browser payload is missing; build stage14c before this regression');
}
const embedded = fs.readFileSync(embeddedPath, 'utf8');
const match = embedded.match(/R"PPHTML\(([\s\S]*)\)PPHTML";/);
if (!match) throw new Error('could not extract embedded normal Browser HTML');
const scriptMatch = match[1].match(/<script>([\s\S]*)<\/script>/);
if (!scriptMatch) throw new Error('embedded normal Browser script is missing');
const script = scriptMatch[1];

class Element {
  constructor(id) {
    this.id = id;
    this.textContent = '';
    this.innerHTML = '';
    this.value = '';
    this.disabled = false;
    this.hidden = false;
    this.onclick = null;
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
  'resultsNav', 'start', 'change'];
global.document = {
  elements: {},
  querySelector(selector) { return this.elements[selector.replace(/^#/, '')] || null; },
  createElement(id) { return new Element(id); },
};
for (const id of ids) document.elements[id] = new Element(id);
global.window = global;
global.$ = selector => document.querySelector(selector);
global.performance = { now: () => 1000 };

const intervals = [];
global.setInterval = callback => { intervals.push(callback); return intervals.length; };
global.clearInterval = () => {};

const timeouts = new Map();
let nextTimeout = 1;
global.setTimeout = (callback, ms) => {
  const id = nextTimeout++;
  timeouts.set(id, { callback, ms });
  return id;
};
global.clearTimeout = id => timeouts.delete(id);

let mode = 'immediate';
let stateCalls = 0;
let devNoticeCalls = 0;
let pendingStateReject = null;
let pendingStateResolve = null;

const state = { lifecycle: 'READY', sessionMode: 'NONE', entries: [], practiceSummaryAvailable: false };
const proposal = { mode: 'LAP_RACE', lapTarget: 2, durationMinutes: 1, finishBehaviour: 'IMMEDIATE',
  startable: true, proposalRevision: 1, entries: [] };
const context = { hasMaster: true, role: 'Race Director' };

function response(value, status = 200) {
  return { ok: status >= 200 && status < 300, status, async json() {
    return JSON.parse(JSON.stringify(value));
  } };
}

global.fetch = async (url, options = {}) => {
  if (url === '/noticeboard') {
    devNoticeCalls++;
    return response({ revision: 1, proposalRevision: proposal.proposalRevision });
  }
  if (url === '/proposal') return response(proposal);
  if (url === '/context') return response(context);
  if (url === '/state') {
    stateCalls++;
    if (mode === 'failure') return response({}, 503);
    if (mode === 'delayed' || mode === 'timeout') {
      return new Promise((resolve, reject) => {
        pendingStateResolve = () => resolve(response(state));
        pendingStateReject = reject;
        if (options.signal) options.signal.addEventListener('abort', () => reject(new Error('AbortError')));
      });
    }
    return response(state);
  }
  return response({});
};

vm.runInThisContext(script, { filename: 'embedded-normal-browser.js' });

const expect = (value, message) => { if (!value) throw new Error(message); };
const settle = async () => { for (let i = 0; i < 12; i++) await Promise.resolve(); };

(async () => {
  // The page invokes its real poll() on load. Let that initial production poll settle.
  await settle();
  mode = 'delayed';
  stateCalls = 0;
  pendingStateResolve = null;
  pendingStateReject = null;

  const first = poll();
  await settle();
  const second = poll();
  await second;
  expect(stateCalls === 1, 'normal polling issued more than one state request while pending');

  // A bounded timeout aborts the real fetch and releases polling for recovery.
  const timeout = [...timeouts.values()].find(x => x.ms === 4000);
  expect(timeout, 'normal request timeout was not scheduled');
  timeout.callback();
  await first;
  expect(stateCalls === 1, 'timed-out normal poll created another request before recovery');

  mode = 'immediate';
  await poll();
  expect(stateCalls === 2, 'normal polling did not recover after timeout');

  // Failed HTTP responses must also clear the guard.
  mode = 'failure';
  await poll();
  mode = 'immediate';
  await poll();
  expect(stateCalls === 4, 'normal polling did not recover after HTTP failure');

  // Model the Development Browser sharing the same server while a normal poll is pending.
  mode = 'delayed';
  const coexistence = poll();
  await settle();
  for (let i = 0; i < 20; i++) {
    await fetch('/noticeboard');
    poll();
  }
  expect(stateCalls === 5, 'normal request growth was unbounded beside Development Browser traffic');
  expect(devNoticeCalls === 20, 'Development Browser coexistence workload was not exercised');
  pendingStateReject(new Error('connection closed'));
  await coexistence;

  console.log('Stage 14C normal Browser polling PASS: single in-flight cycle, timeout/abort recovery, HTTP failure recovery, Development Browser coexistence bounded');
})().catch(error => {
  console.error('Stage 14C normal Browser polling FAIL:', error.stack || error.message);
  process.exitCode = 1;
});
