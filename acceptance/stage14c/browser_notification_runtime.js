const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const normal = fs.readFileSync(path.join(ROOT, 'firmware', 'browser', 'normal_browser.html'), 'utf8');
const embedded = fs.readFileSync(path.join(ROOT, 'firmware', 'include', 'pp', 'normal_browser_page.inc'), 'utf8');
const browser = fs.readFileSync(path.join(ROOT, 'firmware', 'include', 'pp', 'browser_interface.h'), 'utf8');
const expect = (value, message) => { if (!value) throw new Error(message); };

expect(normal.includes("new EventSource('/events')"), 'normal Browser does not open the SSE notification endpoint');
expect(embedded.includes("new EventSource('/events')"), 'embedded normal Browser does not contain the SSE notification client');
expect(browser.includes('{"/events", HTTP_GET, events, nullptr}'), 'production HTTP server does not register /events');
expect(browser.includes('httpd_queue_work(server_, &BrowserInterface::eventWork, work)'), 'notification work is not queued through the HTTP task with owned work context');
expect(browser.includes('struct SseSession'), 'SSE has no heap-owned session record');
expect(browser.includes('struct SseWork'), 'SSE work has no owned generation context');
expect(browser.includes('freeSseSession'), 'SSE session has no HTTPD cleanup callback');
expect(browser.includes('httpd_sess_set_ctx'), 'SSE session is not registered with HTTPD cleanup');
expect(browser.includes('httpd_sess_get_ctx'), 'SSE send path does not validate the registered session');
expect(browser.includes('session->generation != generation'), 'SSE work has no generation validation');
expect(browser.includes('httpd_sess_trigger_close'), 'SSE invalidation has no close path');
expect(browser.includes('frame + header'), 'SSE later sends are not consistently chunk-framed');
expect(browser.includes(': ping\\n\\n'), 'SSE heartbeat is missing');
expect(browser.includes('frame[header + length] = \'\\r\''), 'SSE chunk terminator is missing');
expect(browser.includes('session->workQueued'), 'notification work has no bounded/coalesced queue guard');
expect(browser.includes('session->sentRevision'), 'notification stream has no per-client revision coalescing state');
expect(browser.includes('[SSE] notice_changed revision='), 'SSE source transition diagnostic is missing');
expect(browser.includes('[SSE] queued generation='), 'SSE queue diagnostic is missing');
expect(browser.includes('[SSE] sent generation='), 'SSE send diagnostic is missing');
expect(normal.includes("[SSE] refresh_started revision="), 'Browser refresh-start diagnostic is missing');
expect(normal.includes("[SSE] refresh_rendered revision="), 'Browser refresh-complete diagnostic is missing');
expect(normal.includes('serverUs'), 'SSE notification does not carry the diagnostic server timestamp');
expect(!normal.includes('setInterval(poll,1000)'), 'normal connected-state polling must not compete with SSE invalidation');
expect(normal.includes('pollDirty=false'), 'notification-during-collection dirty state is missing');
expect(normal.includes("poll('reconnect')"), 'SSE reconnect does not reconstruct authoritative state');
expect(normal.includes("stream.addEventListener('presentation'") && normal.includes('S.startPresentation.redLightsLit'), 'authoritative presentation stream is missing from the Browser');
expect(!normal.includes('schedulePresentation') && !normal.includes('pTimeOffsetUs'), 'Browser retains an independent start schedule/time basis');

// Exercise the lifecycle model represented by the production session fields.
// This deliberately covers fd reuse: a stale work item from generation 1
// must not send on generation 2 even when the operating system recycles fd 56.
const sessionModel = { active: null, nextGeneration: 0, queued: false, sends: 0, stale: 0 };
const connect = fd => {
  const generation = ++sessionModel.nextGeneration;
  sessionModel.active = { fd, generation, valid: true };
  return generation;
};
const queue = generation => {
  if (!sessionModel.active || !sessionModel.active.valid || sessionModel.queued) return false;
  sessionModel.queued = true;
  sessionModel.queuedGeneration = generation;
  return true;
};
const disconnect = () => {
  if (sessionModel.active) sessionModel.active.valid = false;
  sessionModel.active = null;
  sessionModel.queued = false;
};
const send = () => {
  const workGeneration = sessionModel.queuedGeneration;
  sessionModel.queued = false;
  if (!sessionModel.active || !sessionModel.active.valid ||
      sessionModel.active.generation !== workGeneration) {
    sessionModel.stale++;
    return false;
  }
  sessionModel.sends++;
  return true;
};
const g1 = connect(56);
expect(queue(g1), 'first SSE generation was not queueable');
disconnect();
const g2 = connect(56); // fd reuse must not make g1 work valid again
expect(!send(), 'stale queued SSE work sent after fd/session reuse');
expect(sessionModel.stale === 1, 'stale SSE work was not discarded');
expect(queue(g2), 'replacement SSE generation was not queueable');
expect(send(), 'current SSE generation was not sent');
expect(sessionModel.sends === 1, 'unexpected SSE send count after reconnect');
disconnect();
expect(!queue(g2), 'closed SSE session accepted more work');

// A stop/reopen cycle has the same invalidation boundary as disconnect.
const g3 = connect(57);
expect(queue(g3), 'reopened SSE generation was not queueable');
disconnect();
expect(!send(), 'pending SSE work survived server/session stop');
expect(sessionModel.stale === 2, 'stop did not discard stale SSE work');

// Ordinary HTTP routes remain registered while the persistent stream exists.
expect(browser.includes('{"/state", HTTP_GET, state, nullptr}'), 'state route missing beside SSE');
expect(browser.includes('{"/proposal", HTTP_GET, proposalRoute, nullptr}'), 'proposal route missing beside SSE');
expect(browser.includes('{"/context", HTTP_GET, contextRoute, nullptr}'), 'context route missing beside SSE');

// Execute the exact browser-side event dispatch fragment with a minimal poll
// guard. Five coalesced notifications while one refresh is in flight must
// start one refresh, not five concurrent batches.
let pollStarts = 0;
let polling = false;
let pollDirty = false;
let finishPoll;
const poll = () => {
  if (polling) { pollDirty = true; return; }
  polling = true;
  pollStarts++;
  finishPoll = () => {
    polling = false;
    if (pollDirty) { pollDirty = false; poll(); }
  };
};
let reconnect;
let stream;
global.EventSource = class {
  static OPEN = 1;
  static CONNECTING = 0;
  static CLOSED = 2;
  constructor(url) { this.url = url; this.readyState = 1; stream = this; }
  addEventListener(name, callback) { this[name] = callback; }
};
global.window = { poll };
global.diagnosticObservation = () => {};
global.setTimeout = callback => { reconnect = callback; return 1; };
const start = normal.indexOf('let eventStream=null;let eventStreamWatchdog=null;function eventStreamIsOpen()');
const end = normal.indexOf("$('#logo').onclick", start);
expect(start >= 0 && end > start, 'could not isolate production notification client');
eval(normal.slice(start, end)); // eslint-disable-line no-eval
openEventStream();
stream.onmessage();
stream.onmessage();
stream.onmessage();
stream.onmessage();
stream.onmessage();
expect(pollStarts === 1, `notification burst started ${pollStarts} refreshes`);
expect(pollDirty, 'notification burst did not retain a dirty follow-up');
finishPoll();
expect(pollStarts === 2, 'dirty notification did not trigger one immediate follow-up collection');
stream.readyState = EventSource.CLOSED;
stream.onerror();
expect(typeof reconnect === 'function', 'closed notification stream did not schedule reconnect');
reconnect();
expect(stream && stream.url === '/events', 'notification stream did not reconnect to /events');

console.log('Stage 14C Browser notification PASS: production /events client/server present, invalidations coalesced, dirty follow-up retained, reconnect reconstruction scheduled, no normal polling loop');
