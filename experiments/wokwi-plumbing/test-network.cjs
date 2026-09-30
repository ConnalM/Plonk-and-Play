// Run with Node.js 22+ while the browser simulator and private gateway are running.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const base = process.argv[2] || 'http://127.0.0.1:9080';
const url = new URL('/ws', base); url.protocol = 'ws:';
const results = { started: new Date().toISOString(), base, checks: [] };
const sockets = [];
function event(socket, name) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => finish(new Error(`${name} timed out`)), 10000);
    const success = e => finish(null, e);
    const error = () => finish(new Error(`WebSocket error awaiting ${name}`));
    function finish(err, value) {
      clearTimeout(timer);
      socket.removeEventListener(name, success);
      socket.removeEventListener('error', error);
      err ? reject(err) : resolve(value);
    }
    socket.addEventListener(name, success, { once: true });
    socket.addEventListener('error', error, { once: true });
  });
}
async function connect() {
  const s = new WebSocket(url); sockets.push(s);
  await event(s, 'open'); return s;
}
async function echo(s, text) {
  assert.equal(s.readyState, WebSocket.OPEN);
  const received = event(s, 'message'); s.send(text);
  const actual = (await received).data; assert.equal(actual, text);
  return { sent: text, received: actual };
}
async function close(s) {
  const closed = event(s, 'close'); s.close(1000, 'test complete');
  await closed; assert.equal(s.readyState, WebSocket.CLOSED);
}
async function run() {
  const response = await fetch(new URL('/health', base), { signal: AbortSignal.timeout(10000) });
  assert.equal(response.status, 200);
  const health = await response.json();
  assert.deepEqual(health, { ok: true, probe: 'pp-wokwi-plumbing-v1', websocket: '/ws' });
  results.checks.push({ test: 'HTTP health', status: 'PASS', response: health });
  const a = await connect();
  results.checks.push({ test: 'Exact WebSocket echo', status: 'PASS', ...await echo(a, 'single-client: exact echo 0123') });
  const b = await connect();
  assert.equal(a.readyState, WebSocket.OPEN);
  assert.equal(b.readyState, WebSocket.OPEN);
  const messagesA = [], messagesB = [];
  a.addEventListener('message', e => messagesA.push(e.data));
  b.addEventListener('message', e => messagesB.push(e.data));
  const echoes = await Promise.all([echo(a, 'client-A: alpha-123'), echo(b, 'client-B: beta-456')]);
  results.checks.push({ test: 'Two simultaneous independent clients', status: 'PASS', echoes });
  await close(a);
  const survivor = await echo(b, 'client-B: still alive after A disconnects');
  assert.deepEqual(messagesA, ['client-A: alpha-123']);
  assert.deepEqual(messagesB, ['client-B: beta-456', survivor.sent]);
  results.checks.push({ test: 'Survivor after other client disconnects', status: 'PASS', ...survivor });
  await close(b);
}
run().catch(e => { results.error = e.stack; process.exitCode = 1; }).finally(() => {
  for (const s of sockets) if (s.readyState === WebSocket.OPEN) s.close();
  results.finished = new Date().toISOString();
  fs.mkdirSync(path.join(__dirname, '.pio'), { recursive: true });
  fs.writeFileSync(path.join(__dirname, '.pio', 'network-test-results.json'), JSON.stringify(results, null, 2));
  console.log(JSON.stringify(results, null, 2));
});
