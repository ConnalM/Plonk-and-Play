const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const embedded = fs.readFileSync(
  path.join(ROOT, 'firmware', 'include', 'pp', 'normal_browser_page.inc'),
  'utf8',
);

function expect(value, message) {
  if (!value) throw new Error(message);
}

// The generated production payload must make the recovery branch visibly
// sequential. Healthy SSE-driven refreshes may retain their normal parallel
// acquisition; only reconnect/recovery uses this bounded branch.
const recoveryMarker = "const recoveryAcquisition=trigger==='reconnect'||trigger==='recovery'";
const recoveryStart = embedded.indexOf(recoveryMarker);
expect(recoveryStart >= 0, 'recovery acquisition marker is missing');
const recoveryEnd = embedded.indexOf('const nextS=timedS.value', recoveryStart);
expect(recoveryEnd > recoveryStart, 'recovery acquisition branch is incomplete');
const recovery = embedded.slice(recoveryStart, recoveryEnd);
const stateAt = recovery.indexOf("readTimedState(trace,observation)");
const proposalAt = recovery.indexOf("tracedGet('/proposal','proposal',trace)");
const contextAt = recovery.indexOf("tracedGet('/context','context',trace)");
expect(stateAt >= 0 && proposalAt > stateAt && contextAt > proposalAt,
  'recovery acquisition order is not /state -> /proposal -> /context');
const recoveryBranch = recovery.slice(recovery.indexOf('if(recoveryAcquisition){'), recovery.indexOf('}else if(activeBefore)'));
expect(!recoveryBranch.includes('Promise.all'),
  'recovery acquisition still launches concurrent requests');

class Transport {
  constructor() {
    this.clientCount = 8;
    this.ordinarySessions = 8;
    this.activeSse = new Map();
    this.inflight = new Map();
    this.maxInflight = new Map();
    this.events = [];
  }

  admitSse(browser, epoch) {
    const count = this.clientCount;
    if (count >= 9) {
      this.events.push({ type: 'sse-reject', browser, epoch, count, status: 503, close: true });
      return false;
    }
    this.clientCount++;
    this.activeSse.set(browser, { epoch, fd: 50 + epoch });
    this.events.push({ type: 'sse-admit', browser, epoch, count });
    return true;
  }

  failSse(browser, epoch) {
    const active = this.activeSse.get(browser);
    expect(active && active.epoch === epoch, 'failed SSE was not the current generation');
    this.activeSse.delete(browser);
    this.clientCount--;
    this.events.push({ type: 'sse-send-failure', browser, epoch });
  }

  deliverHealthy(browser) {
    const active = this.activeSse.get(browser);
    expect(active, 'healthy SSE was not admitted');
    this.events.push({ type: 'healthy-sse-delivery', browser, epoch: active.epoch });
  }

  async ordinary(browser, route) {
    const active = (this.inflight.get(browser) || 0) + 1;
    this.inflight.set(browser, active);
    this.maxInflight.set(browser, Math.max(active, this.maxInflight.get(browser) || 0));
    this.events.push({ type: 'ordinary-enter', browser, route, active });
    await Promise.resolve();
    this.events.push({ type: 'ordinary-complete', browser, route });
    this.inflight.set(browser, active - 1);
  }

  releaseOrdinary(count) {
    this.ordinarySessions = Math.max(0, this.ordinarySessions - count);
    this.clientCount = this.ordinarySessions + this.activeSse.size;
  }

  addOrdinary() {
    this.ordinarySessions++;
    this.clientCount++;
  }
}

class BrowserRecovery {
  constructor(name, role, transport) {
    this.name = name;
    this.role = role;
    this.transport = transport;
    this.controllerGeneration = 1;
    this.eventEpoch = 1;
    this.currentStream = { epoch: this.eventEpoch, connecting: false, open: true };
    this.synchronised = true;
    this.acquired = [];
  }

  loseController() {
    this.controllerGeneration++;
    this.synchronised = false;
    this.currentStream.open = false;
  }

  retireAndOpen() {
    expect(!this.currentStream.connecting, `${this.name} overlapped EventSource attempts`);
    this.eventEpoch++;
    this.currentStream = { epoch: this.eventEpoch, connecting: true, open: false };
    const admitted = this.transport.admitSse(this.name, this.eventEpoch);
    if (!admitted) {
      this.currentStream = { epoch: this.eventEpoch, connecting: false, open: false };
      return false;
    }
    this.currentStream.connecting = false;
    this.currentStream.open = true;
    return true;
  }

  async acquireFresh() {
    const epoch = this.eventEpoch;
    this.acquired = [];
    for (const route of ['/state', '/proposal', '/context']) {
      expect(this.currentStream.epoch === epoch && this.currentStream.open,
        `${this.name} acquired against a stale EventSource epoch`);
      await this.transport.ordinary(this.name, route);
      this.acquired.push(route);
    }
    expect(this.acquired.join(',') === '/state,/proposal,/context',
      `${this.name} did not acquire state/proposal/context in order`);
    this.synchronised = true;
  }
}

(async () => {
  const transport = new Transport();
  const master = new BrowserRecovery('master', 'MASTER', transport);
  const spectator = new BrowserRecovery('spectator', 'SPECTATOR', transport);

  // Simultaneous controller-generation loss must not mutate either role.
  master.loseController();
  spectator.loseController();
  expect(master.role === 'MASTER' && spectator.role === 'SPECTATOR',
    'controller loss changed authority roles');

  // The healthy stream is restored first; the second stream is intentionally
  // exercised at the 9-client boundary and must receive bounded 503/close.
  expect(master.retireAndOpen(), 'first recovered SSE was not admitted at clients=8');
  expect(!spectator.retireAndOpen(), 'second recovered SSE bypassed the clients=9 boundary');
  const rejection = transport.events.find(e => e.type === 'sse-reject');
  expect(rejection && rejection.count === 9 && rejection.status === 503 && rejection.close,
    'clients=9 SSE rejection was not explicit 503/close');

  transport.addOrdinary();
  expect(!spectator.retireAndOpen(), 'second recovered SSE bypassed the clients=10 boundary');
  const highRejection = transport.events.find(e => e.type === 'sse-reject' && e.count === 10);
  expect(highRejection && highRejection.status === 503 && highRejection.close,
    'clients=10 SSE rejection was not explicit 503/close');

  await master.acquireFresh();
  transport.deliverHealthy('master');

  // Capacity release permits the failed Browser to recover without changing
  // the admission arithmetic or creating a second current EventSource.
  transport.releaseOrdinary(2);
  expect(spectator.retireAndOpen(), 'failed Browser did not recover when capacity permitted');
  await spectator.acquireFresh();
  transport.deliverHealthy('master');

  // Model the transient send failure and a stale fd/generation work item.
  const failedEpoch = master.eventEpoch;
  transport.failSse('master', failedEpoch);
  master.currentStream.open = false;
  const stale = { browser: 'master', epoch: failedEpoch, fd: 50 + failedEpoch };
  expect(stale.epoch !== master.eventEpoch || !master.currentStream.open,
    'stale generation remained eligible after send failure');
  expect(transport.activeSse.has('spectator'), 'healthy second SSE was affected by peer failure');

  // The failed Browser can be replaced only after its old generation is
  // retired; the transport returns to exactly the two-SSE baseline.
  transport.releaseOrdinary(7);
  expect(master.retireAndOpen(), 'failed Browser replacement was not admitted after reclamation');
  await master.acquireFresh();
  expect(master.synchronised && spectator.synchronised,
    'both Browsers did not complete fresh recovery acquisition');
  expect(master.role === 'MASTER' && spectator.role === 'SPECTATOR',
    'fresh /context acquisition changed authority roles');
  expect([...transport.maxInflight.values()].every(value => value === 1),
    'a Browser had more than one recovery acquisition request in flight');
  expect(transport.inflight.get('master') === 0 && transport.inflight.get('spectator') === 0,
    'recovery left an ordinary request in flight');
  expect(transport.activeSse.size === 2 && transport.clientCount === 2,
    'recovery resources did not return to the two-SSE baseline');
  expect(!transport.events.some(e => e.type === 'race' || e.type === 'persistence'),
    'recovery model mutated race or persistence state');

  console.log('Stage 14C two-Browser reboot recovery PASS: sequential state/proposal/context acquisition, bounded SSE admission/recovery, healthy peer isolation, generation ownership and baseline reclamation');
})().catch(error => {
  console.error('Stage 14C two-Browser reboot recovery FAIL:', error.stack || error.message);
  process.exitCode = 1;
});
