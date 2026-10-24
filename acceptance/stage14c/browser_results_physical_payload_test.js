const fs = require('fs');
const vm = require('vm');

const html = fs.readFileSync('firmware/browser/normal_browser.html', 'utf8');
const script = html.match(/<script>([\s\S]*)<\/script>/)[1];

class Element {
  constructor(id) {
    this.id = id; this.textContent = ''; this._innerHTML = '';
    this.value = ''; this.disabled = false; this.hidden = false;
    this.onclick = null; this.listeners = {};
    this.classList = { toggle() {}, add() {}, remove() {} };
  }
  addEventListener(type, fn) { this.listeners[type] = fn; }
  set innerHTML(value) {
    this._innerHTML = String(value);
    for (const match of this._innerHTML.matchAll(/id=["']([^"']+)["']/g)) {
      if (!document.elements[match[1]]) document.elements[match[1]] = new Element(match[1]);
    }
  }
  get innerHTML() { return this._innerHTML; }
  insertAdjacentHTML(_position, value) { this._innerHTML += String(value); }
}

const ids = ['logo', 'head', 'status', 'reason', 'app', 'controlLease', 'overlay', 'notice',
  'setupNav', 'resultsNav', 'trackNav', 'racersNav', 'settingsNav', 'devTab', 'devDrawer',
  'devClose', 'devL1', 'devL2', 'devState', 'devLanes', 'devFact', 'devHealth', 'devHealthMore'];
const elements = Object.fromEntries(ids.map(id => [id, new Element(id)]));
global.document = {
  elements,
  querySelector(selector) {
    if (selector.startsWith('#')) return elements[selector.slice(1)] || null;
    return null;
  },
  querySelectorAll() { return []; }
};
global.window = global;
global.performance = { now: () => 1000 };
global.setInterval = () => 1;
global.EventSource = undefined;

const state = { lifecycle: 'FINISHED', sessionMode: 'LAP_RACE', resultSealed: true,
  practiceSummaryAvailable: false, historySequence: 44, entries: [] };
const proposal = { mode: 'LAP_RACE', lapTarget: 2, startable: false,
  proposalRevision: 1, entries: [] };
const context = { hasMaster: true, isMaster: true, role: 'Race Director' };
const incomplete = { sequence: 44, sealed: true, mode: 'LAP_RACE', lapTarget: 2,
  fastestLap: 1977943, entries: [
    { rank: 1, mugId: 91, lane: 1, laps: 2, classifiedLaps: 2, completed: true,
      finishDurationUs: 0, bestLap: 1977943, lapsBehind: 0 },
    { rank: 2, mugId: 92, lane: 2, laps: 1, classifiedLaps: 1, completed: false,
      finishDurationUs: 0, bestLap: 3400239, lapsBehind: 1 }
  ] };
const physical = { sequence: 44, sealed: true, mode: 'LAP_RACE', lapTarget: 2,
  fastestLap: 1977943, entries: [
    { rank: 1, mugId: 91, lane: 1, laps: 2, classifiedLaps: 2, completed: true,
      finishDurationUs: 4741054, bestLap: 1977943, lapsBehind: 0 },
    { rank: 2, mugId: 92, lane: 2, laps: 1, classifiedLaps: 1, completed: false,
      finishDurationUs: 3400239, bestLap: 3400239, lapsBehind: 1 }
  ] };
let resultRequests = 0;
function response(value, status = 200) {
  return { ok: status >= 200 && status < 300, status,
    async json() { return JSON.parse(JSON.stringify(value)); } };
}
global.fetch = async (url) => {
  if (url === '/state') return response(state);
  if (url === '/proposal') return response(proposal);
  if (url === '/context') return response(context);
  if (url === '/results') return response(++resultRequests === 1 ? incomplete : physical);
  return response({});
};

vm.runInThisContext(script, { filename: 'normal_browser.html' });
const wait = ms => new Promise(resolve => setTimeout(resolve, ms));
(async () => {
  await wait(1100);
  if (resultRequests < 2) throw new Error(`expected bounded retry, got ${resultRequests} request(s)`);
  const rendered = elements.app.innerHTML;
  for (const expected of ['2 / 2', '1 / 2', '4.74 s', '3.40 s']) {
    if (!rendered.includes(expected)) throw new Error(`physical Results payload did not render ${expected}`);
  }
  if (!rendered.includes('<div>2 / 2</div><div>4.74 s</div>') ||
      !rendered.includes('<div>1 / 2</div><div>3.40 s</div>')) {
    throw new Error('physical non-zero finish durations rendered as unavailable');
  }
  console.log(`Normal Browser physical Results payload regression PASS: ${resultRequests} /results requests, both elapsed times rendered`);
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
