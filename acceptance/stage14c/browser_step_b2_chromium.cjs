/* Browser-engine evidence only: intercepted JSON, no firmware or device access. */
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const { chromium } = require('playwright');

const root = path.resolve(__dirname, '..', '..');
const html = fs.readFileSync(path.join(root, 'firmware', 'browser', 'normal_browser.html'), 'utf8');
const evidence = path.join(__dirname, 'evidence', 'browser-step-b2');
fs.mkdirSync(evidence, { recursive: true });

const state = () => ({ lifecycle: 'READY', sessionMode: 'NONE', finishDisplay: { active: false }, entries: [], resultSealed: false });
const context = () => ({ hasMaster: true, isMaster: true, role: 'Race Director' });
const proposal = revision => ({ proposalRevision: revision, mode: 'LAP_RACE', lapTarget: 10, durationMinutes: 5, finishBehaviour: 'IMMEDIATE', startable: true, entries: [{ lane: 1, mugId: 7 }, { lane: 2, mugId: 77 }], supportedModes: [] });
const fullTrack = device => ({
  assignments: [{ lane: 1, device, capability: 8 }, { lane: 2, device: 20, capability: 2 }],
  detectors: [
    { device, capability: 8, observedDevice: device, observedCapability: 8, availability: 'AVAILABLE', health: 'HEALTHY', replacementRequired: false },
    { device: 20, capability: 2, observedDevice: 21, observedCapability: 2, availability: 'MISSING', health: 'FAULT', replacementRequired: true },
    { device: 30, capability: 3, observedDevice: 0, observedCapability: 0, availability: 'UNKNOWN', health: 'UNKNOWN', replacementRequired: false },
  ],
});
const fullRacers = suffix => ({
  racers: [
    { id: 1, builtin: true, name: 'MUG 1' },
    { id: 7, builtin: false, name: 'Åsa & <Racer> ' + '長い名前'.repeat(12) + suffix },
  ],
  assignments: [
    { lane: 1, mugId: 7, name: 'Åsa & <Racer> ' + '長い名前'.repeat(12) + suffix, selectable: true, reconciliationRequired: false, status: 'ACTIVE' },
    { lane: 2, mugId: 77, name: 'Guest 77', selectable: false, reconciliationRequired: true, status: 'UNKNOWN' },
  ],
});

async function mockedPage(browser, viewport, model) {
  const page = await browser.newPage({ viewport });
  const errors = [];
  page.on('pageerror', error => errors.push(String(error)));
  page.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
  await page.route('**/*', async route => {
    const request = route.request();
    const pathname = new URL(request.url()).pathname;
    const json = body => route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify(body) });
    if (pathname === '/') return route.fulfill({ status: 200, contentType: 'text/html', body: html });
    if (pathname === '/events') return route.fulfill({ status: 200, contentType: 'text/event-stream', body: 'event: heartbeat\ndata: {}\n\n' });
    if (request.method() === 'POST') {
      model.posts.push({ pathname, method: request.method() });
      if (pathname.startsWith('/request/')) model.writes.push({ pathname, method: request.method() });
      return route.fulfill({ status: 204 });
    }
    if (pathname === '/state') return json(state());
    if (pathname === '/proposal') return json(proposal(model.revision));
    if (pathname === '/context') return json(context());
    if (pathname === '/track/setup') {
      model.trackRequests += 1;
      if (model.delayTrack) await new Promise(resolve => setTimeout(resolve, model.delayTrack));
      if (model.trackFailure) return route.fulfill({ status: 503, contentType: 'application/json', body: '{}' });
      return json({ proposalRevision: model.revision, laneCount: model.laneCount || 2, ...(model.track || fullTrack(10)) });
    }
    if (pathname === '/racers') {
      model.racerRequests += 1;
      if (model.racersFailure) return route.fulfill({ status: 503, contentType: 'application/json', body: '{}' });
      return json({ proposalRevision: model.revision, ...(model.racers || fullRacers('')) });
    }
    return json({});
  });
  await page.goto('http://pp.test/', { waitUntil: 'domcontentloaded' });
  await page.waitForFunction(() => document.getElementById('status').textContent !== 'CONNECTING');
  model.posts.length = 0;
  return { page, errors };
}

async function assertReadable(page) {
  const result = await page.evaluate(() => {
    const entries = [...document.querySelectorAll('.entry')];
    return {
      documentOverflow: document.documentElement.scrollWidth > window.innerWidth + 1,
      clipped: entries.some(entry => {
        const box = entry.getBoundingClientRect();
        return box.width <= 0 || box.right > window.innerWidth + 1 || box.left < -1;
      }),
      overlap: entries.some(entry => {
        const children = [...entry.children].map(child => child.getBoundingClientRect());
        return children.length > 1 && children[0].right > children[1].left + 1;
      }),
    };
  });
  assert.equal(result.documentOverflow, false, 'unexpected document-level horizontal overflow');
  assert.equal(result.clipped, false, 'entry is clipped by the viewport');
  assert.equal(result.overlap, false, 'entry columns overlap');
}

async function show(page, selector, title, readyText, screenshot) {
  await page.locator(selector).click();
  await page.getByRole('heading', { name: title, exact: true }).waitFor();
  await page.waitForFunction(text => document.getElementById('app').innerText.includes(text), readyText);
  await page.screenshot({ path: screenshot, fullPage: true });
  await assertReadable(page);
}

(async () => {
  const browser = await chromium.launch({ headless: true });
  try {
    for (const viewport of [
      { name: 'desktop', width: 1440, height: 900 },
      { name: 'tablet', width: 768, height: 1024 },
      { name: 'mobile', width: 390, height: 844 },
    ]) {
      const model = { revision: 7, posts: [], writes: [], trackRequests: 0, racerRequests: 0 };
      const { page, errors } = await mockedPage(browser, viewport, model);
      await show(page, '#trackNav', 'Track Setup', 'HEALTHY', path.join(evidence, 'track-' + viewport.name + '.png'));
      assert.match(await page.locator('#app').innerHTML(), /HEALTHY.*FAULT.*UNKNOWN/s);
      assert.equal(model.writes.length, 0, 'Track view issued a write request');
      await show(page, '#racersNav', 'Racers', 'Åsa', path.join(evidence, 'racers-' + viewport.name + '.png'));
      assert.match(await page.locator('#app').innerHTML(), /Åsa &amp; &lt;Racer&gt;/);
      assert.equal(model.writes.length, 0, 'Racers view issued a write request');
      assert.deepEqual(errors, [], 'browser exception or console error');
      await page.close();
    }

    const empty = { revision: 7, posts: [], writes: [], trackRequests: 0, racerRequests: 0, track: { assignments: [], detectors: [] }, racers: { racers: [], assignments: [] } };
    const emptyPage = await mockedPage(browser, { width: 1440, height: 900 }, empty);
    await show(emptyPage.page, '#trackNav', 'Track Setup', 'No detector status is available.', path.join(evidence, 'track-empty.png'));
    assert.match(await emptyPage.page.locator('#app').innerText(), /No detector status is available/);
    await show(emptyPage.page, '#racersNav', 'Racers', 'No saved racer profiles are available.', path.join(evidence, 'racers-empty.png'));
    assert.match(await emptyPage.page.locator('#app').innerText(), /No saved racer profiles are available/);
    assert.deepEqual(emptyPage.errors, [], 'empty-state browser exception');
    await emptyPage.page.close();

    const unavailable = { revision: 7, posts: [], writes: [], trackRequests: 0, racerRequests: 0, trackFailure: true, racersFailure: true };
    const unavailablePage = await mockedPage(browser, { width: 1440, height: 900 }, unavailable);
    await show(unavailablePage.page, '#trackNav', 'Track Setup', 'Detector information is unavailable.', path.join(evidence, 'track-unavailable.png'));
    assert.match(await unavailablePage.page.locator('#app').innerText(), /Detector information is unavailable/);
    const afterFailure = unavailable.trackRequests;
    await unavailablePage.page.waitForTimeout(250);
    assert.equal(unavailable.trackRequests, afterFailure, 'failed Track request retried without user action');
    await unavailablePage.page.locator('#b2TrackRetry').click();
    await unavailablePage.page.waitForTimeout(50);
    assert.equal(unavailable.trackRequests, afterFailure + 1, 'Track retry did not issue exactly one request');
    await show(unavailablePage.page, '#racersNav', 'Racers', 'Racer information is unavailable.', path.join(evidence, 'racers-unavailable.png'));
    assert.match(await unavailablePage.page.locator('#app').innerText(), /Racer information is unavailable/);
    assert.deepEqual(unavailablePage.errors, [], 'unavailable-state browser exception');
    await unavailablePage.page.close();

    const revision = { revision: 7, posts: [], writes: [], trackRequests: 0, racerRequests: 0, track: fullTrack(10), racers: fullRacers('') };
    const revisionPage = await mockedPage(browser, { width: 768, height: 1024 }, revision);
    await show(revisionPage.page, '#trackNav', 'Track Setup', 'HEALTHY', path.join(evidence, 'track-revision-7.png'));
    revision.revision = 8; revision.track = fullTrack(88);
    await revisionPage.page.evaluate(async () => { await window.poll('reconnect'); });
    await revisionPage.page.getByText('device 88 / capability 8').waitFor();
    assert.ok(revision.trackRequests >= 2, 'stale Track cache was not invalidated by proposal revision');
    await show(revisionPage.page, '#racersNav', 'Racers', 'Åsa', path.join(evidence, 'racers-revision-8.png'));
    revision.revision = 9; revision.racers = fullRacers(' · refreshed');
    await revisionPage.page.evaluate(async () => { await window.poll('reconnect'); });
    await revisionPage.page.getByText(/refreshed/).waitFor();
    assert.ok(revision.racerRequests >= 2, 'stale Racer cache was not invalidated by proposal revision');
    assert.deepEqual(revisionPage.errors, [], 'revision browser exception');
    await revisionPage.page.close();

    const delayed = { revision: 7, posts: [], writes: [], trackRequests: 0, racerRequests: 0, delayTrack: 150 };
    const delayedPage = await mockedPage(browser, { width: 390, height: 844 }, delayed);
    await delayedPage.page.locator('#trackNav').click();
    await delayedPage.page.locator('#racersNav').click();
    await delayedPage.page.getByRole('heading', { name: 'Racers' }).waitFor();
    await delayedPage.page.waitForTimeout(200);
    assert.match(await delayedPage.page.locator('#app').innerText(), /Saved racers/);
    assert.deepEqual(delayedPage.errors, [], 'in-flight navigation browser exception');
    await delayedPage.page.close();
  } finally {
    await browser.close();
  }
  console.log('Step B2 Chromium PASS: desktop/tablet/mobile Track and Racers screenshots, read-only routes, errors/retry, revision invalidation and in-flight navigation');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
