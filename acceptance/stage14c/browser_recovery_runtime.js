const fs = require('fs');
const vm = require('vm');

const source = fs.readFileSync('firmware/include/pp/browser_interface.h', 'utf8');
const html = source.match(/static const char html\[\] = R"HTML\(([\s\S]*?)\)HTML";/)[1];
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];

class Element {
  constructor(id) { this.id = id; this.textContent = ''; this.disabled = false; this.hidden = false; this._innerHTML = ''; }
  set innerHTML(value) {
    this._innerHTML = value;
    for (const id of String(value).matchAll(/(?:id|name)=['\"]([^'\"]+)['\"]/g)) {
      if (global.document && !global.document.elements[id[1]]) global.document.elements[id[1]] = new Element(id[1]);
    }
  }
  get innerHTML() { return this._innerHTML; }
}
const ids = [
  'connection','role','human','request','state','fact','bootstrap','start','pause','honour','grid',
  'raceAgain','target3','lane1','lane2','resetTest','results','details','history','back','home',
  'clearAll','clearHistory','clearLane1','clearLane2','clearTrack','endRace','enduranceFinish',
  'enduranceMinutes','enduranceResume','enduranceSelect','practiceEnd','practiceResume','practiceSelect',
  'restartRace','resultView','stage13Fact','stage13Lights'
];
global.document = { elements: {}, querySelector(s) { return this.elements[s.replace(/^#/, '')] || null; } };
for (const id of ids) document.elements[id] = new Element(id);
global.$ = s => document.querySelector(s);
global.window = global;
global.performance = { now: () => Date.now() };
const realSetTimeout = global.setTimeout;
const realClearTimeout = global.clearTimeout;
global.setTimeout = (fn, _ms) => realSetTimeout(fn, 1);
global.clearTimeout = realClearTimeout;
global.setInterval = () => 1;
global.clearInterval = () => {};

let phase = 'initial-timeout';
let active = 0;
let maximumActive = 0;
global.fetch = (path, options = {}) => {
  ++active; maximumActive = Math.max(maximumActive, active);
  const finish = (value, delay = 0) => new Promise(resolve => realSetTimeout(() => { --active; resolve(value); }, delay));
  if (phase === 'initial-timeout' && path === '/noticeboard') {
    return new Promise((_resolve, reject) => {
      options.signal.addEventListener('abort', () => { --active; reject(new Error('aborted')); }, { once: true });
    });
  }
  if (path === '/noticeboard') return finish({ ok: true, status: 200, json: async () => ({ revision: 1 }) }, phase === 'overlap' ? 10 : 0);
  if (path === '/context') return finish({ ok: true, status: 200, json: async () => ({ role: 'Spectator', hasMaster: false }) });
  if (path === '/state') return finish({ ok: true, status: 200, json: async () => ({ lifecycle: 'RACING', sessionMode: 'ENDURANCE', entries: [], raceIntegrity: 'OK', resultValid: true }) });
  if (path === '/fact') return finish({ ok: true, status: 200, json: async () => ({ type: 'NONE', revision: 0 }) });
  return finish({ ok: true, status: 200, json: async () => ({}) });
};

vm.runInThisContext(script, { filename: 'browser-interface-html.js' });

function requireValue(value, message) { if (!value) throw new Error(message); }
async function settle() { await new Promise(resolve => realSetTimeout(resolve, 15)); }

(async () => {
  await settle();
  phase = 'recovery';
  try { await global.poll(); } catch (error) { throw new Error('poll threw: '+error.stack); }
  requireValue(document.elements.connection.textContent === 'synchronised', 'polling did not recover after connectivity returned (connection='+document.elements.connection.textContent+', active='+active+')');
  phase = 'overlap'; maximumActive = 0;
  await Promise.all([global.poll(), global.poll()]);
  requireValue(maximumActive === 1, 'poll calls overlapped during recovery');
  console.log('Stage 14C Browser recovery runtime PASS: timeout clears polling, recovery succeeds, overlap remains bounded');
})().catch(error => { console.error('Stage 14C Browser recovery runtime FAIL:', error.message); process.exitCode = 1; });
