const fs = require('fs');
const vm = require('vm');

const html = fs.readFileSync('firmware/browser/normal_browser.html', 'utf8');
const scripts = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(match => match[1]);
const extension = scripts.at(-1);
if (!extension.includes("G('/track/setup')") || !extension.includes("G('/racers')")) {
  throw new Error('Step B2 does not use both authoritative read-only endpoints');
}
for (const forbidden of ['/request/setup', '/request/mug/create', 'applyTrackSetup', '<select']) {
  if (extension.includes(forbidden)) throw new Error('Step B2 extension retained a write/configuration path: ' + forbidden);
}

const wait = () => new Promise(resolve => setTimeout(resolve, 0));
async function renderViews({ track, racers, unavailable = false, laneCount = 2, isMaster = false }) {
  const requests = [];
  const elements = { trackNav: {}, racersNav: {} };
  const context = {
    A: { innerHTML: '' },
    P: { proposalRevision: 7, entries: [{ lane: 1, mugId: 7 }, { lane: 2, mugId: 77 }] },
    C: { isMaster },
    V: 'track',
    window: { render() {} },
    render() { context.window.render(); },
    $(selector) { return elements[selector.replace(/^#/, '')] || null; },
    G: async path => {
      requests.push(path);
      if (unavailable) throw new Error('unavailable');
      if (path === '/track/setup') return { proposalRevision: 7, laneCount, ...track };
      if (path === '/racers') return { proposalRevision: 7, ...racers };
      throw new Error('unexpected path ' + path);
    },
    Number, String, Array, Object, setTimeout, clearTimeout,
  };
  vm.runInNewContext(extension, context, { filename: 'step_b2_browser_extension.js' });
  context.window.render();
  await wait();
  await wait();
  const trackHtml = context.A.innerHTML;
  context.V = 'racers';
  context.window.render();
  await wait();
  await wait();
  return { trackHtml, racersHtml: context.A.innerHTML, requests };
}

(async () => {
  const full = await renderViews({
    track: {
      assignments: [{ lane: 1, device: 10, capability: 1 }, { lane: 2, device: 20, capability: 2 }],
      detectors: [
        { device: 10, capability: 1, observedDevice: 10, observedCapability: 1, availability: 'AVAILABLE', health: 'HEALTHY', replacementRequired: false },
        { device: 20, capability: 2, observedDevice: 21, observedCapability: 2, availability: 'MISSING', health: 'FAULT', replacementRequired: true },
        { device: 30, capability: 3, observedDevice: 0, observedCapability: 0, availability: 'UNKNOWN', health: 'UNKNOWN', replacementRequired: false },
      ],
    },
    racers: {
      racers: [{ id: 1, builtin: true, name: 'MUG 1' }, { id: 7, builtin: false, name: 'Åsa & <Racer>' }],
      assignments: [
        { lane: 1, mugId: 7, name: 'Åsa & <Racer>', selectable: true, reconciliationRequired: false, status: 'ACTIVE' },
        { lane: 2, mugId: 77, name: 'Guest 77', selectable: false, reconciliationRequired: true, status: 'UNKNOWN' },
      ],
    },
    laneCount: 3,
  });
  for (const expected of ['LANE 1', 'LANE 2', 'HEALTHY', 'FAULT', 'UNKNOWN', 'Observed device 21 / capability 2', 'Additional lane configuration is not available']) {
    if (!full.trackHtml.includes(expected)) throw new Error('full Track view missing ' + expected);
  }
  for (const expected of ['Saved racers', 'Current race entries', 'Åsa &amp; &lt;Racer&gt;', 'Guest 77', 'reconciliation required']) {
    if (!full.racersHtml.includes(expected)) throw new Error('full Racers view missing ' + expected);
  }
  if (full.requests.filter(path => path === '/track/setup').length !== 1 ||
      full.requests.filter(path => path === '/racers').length !== 1) {
    throw new Error('unexpected refresh count: ' + full.requests.join(', '));
  }

  const empty = await renderViews({ track: { assignments: [], detectors: [] }, racers: { racers: [], assignments: [] }, isMaster: false });
  if (!empty.trackHtml.includes('No detector status is available.') || !empty.racersHtml.includes('No saved racer profiles are available.') ||
      !empty.racersHtml.includes('View-only Browser presentation')) {
    throw new Error('empty or view-only state is not explicit');
  }

  const malformed = await renderViews({ track: { assignments: null, detectors: null }, racers: { racers: null, assignments: null } });
  if (!malformed.trackHtml.includes('Unknown') || !malformed.racersHtml.includes('MUG 7')) {
    throw new Error('malformed payload treatment is not bounded');
  }

  const unavailable = await renderViews({ track: {}, racers: {}, unavailable: true });
  if (!unavailable.trackHtml.includes('Detector information is unavailable.') ||
      !unavailable.racersHtml.includes('Racer information is unavailable.')) {
    throw new Error('unavailable payload state is not explicit');
  }

  console.log('Step B2 read-only Browser runtime PASS: Track/Racers full, empty, unavailable, malformed, UTF-8, view-only and two-lane states');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
