const fs = require('fs');
const vm = require('vm');

const payloadPath = 'firmware/include/pp/normal_browser_page.inc';
const payload = fs.readFileSync(payloadPath, 'utf8');
const match = payload.match(/R"PPHTML\(([\s\S]*)\)PPHTML"/);
if (!match) throw new Error('embedded normal Browser payload is missing');

const scripts = [];
const scriptRe = /<script(?: [^>]*)?>([\s\S]*?)<\/script>/gi;
let scriptMatch;
while ((scriptMatch = scriptRe.exec(match[1])) !== null) scripts.push(scriptMatch[1]);
if (!scripts.length) throw new Error('embedded Browser payload contains no script');
for (const [index, script] of scripts.entries()) {
  try { new vm.Script(script, { filename: `${payloadPath}#script${index + 1}` }); }
  catch (error) { throw new Error(`embedded Browser JavaScript parse failed: ${error.message}`); }
}

const required = [
  ['Settings firmware identity', 'Current firmware:'],
  ['local file chooser', 'type="file"'],
  ['explicit update confirmation', 'Install this firmware update and restart P&P?'],
  ['transfer progress', 'Transferring update to P&P…'],
  ['validation progress', 'Checking update…'],
  ['reconnect progress', 'Restarting P&P… Reconnecting…'],
  ['begin endpoint', '/request/update/begin'],
  ['upload endpoint', '/request/update/upload'],
  ['validate endpoint', '/request/update/validate'],
  ['install endpoint', '/request/update/install'],
  ['online check endpoint', '/update/check-online'],
  ['online update endpoint', '/request/update/online'],
  ['restart action', 'Restart P&amp;P'],
  ['reset settings action', 'Reset settings'],
  ['erase everything action', 'Erase everything'],
  ['ready-to-install state', 'The update is ready to install.'],
  ['rollback success message', 'The previous firmware was restored.'],
  ['rollback support message', 'Contact support.'],
  ['settled authority gate', 'Only the controlling Race Director can update P&P while the race is settled.'],
];
for (const [label, marker] of required) {
  if (!match[1].includes(marker)) throw new Error(`${label}: missing generated-payload marker`);
}
const order = ['/request/update/begin', '/request/update/upload', '/request/update/validate', '/request/update/install']
  .map((route) => match[1].indexOf(route));
if (order.some((offset) => offset < 0) || order[0] >= order[1] || order[1] >= order[2] || order[2] >= order[3]) {
  throw new Error('customer update workflow endpoint order is not begin -> upload -> validate -> install');
}
for (const marker of ['XMLHttpRequest', 'onabort', 'UPDATE_ALREADY_IN_PROGRESS', 'DOWNGRADE_REJECTED',
  'INCOMPATIBLE_TARGET', 'The upload was interrupted. The current firmware is unchanged.',
  'updateInstallPending', 'updateLastKnownFirmware', 'id="updateControllerState"', 'id="updateNotice"']) {
  if (!match[1].includes(marker)) throw new Error(`customer update workflow missing ${marker}`);
}
if (!match[1].includes("const installAllowed=candidateReady&&canControl&&!updateUiBusy&&!updateInstallPending")) {
  throw new Error('install control is not gated by current authority/lifecycle and candidate readiness');
}
if (!match[1].includes('accept=".ppu,.bin,application/octet-stream"')) {
  throw new Error('local update file chooser does not accept signed .ppu packages');
}
if (!match[1].includes('id="updateInstall"') || !match[1].includes('install.hidden=!installAllowed')) {
  throw new Error('install control is not updated through the install-authority gate');
}
const installStart = match[1].indexOf('async function updateInstall(){');
const installEnd = match[1].indexOf('let onlineOffer=', installStart);
if (installStart < 0 || installEnd < 0) throw new Error('install action missing');
const install = match[1].slice(installStart, installEnd);
for (const marker of ["if(updateUiBusy||updateInstallPending)return", "if(!C?.isMaster)", "if(!['READY','FINISHED'].includes(S?.lifecycle))", "if(!window.confirm('Install the validated update and restart P&P now?'))return", "/request/update/install"]) {
  if (!install.includes(marker)) throw new Error(`install action missing ${marker}`);
}
if (!(install.indexOf("if(!window.confirm('Install the validated update and restart P&P now?'))return") < install.indexOf("/request/update/install"))) {
  throw new Error('confirmation dismissal could send an install POST');
}
if (!match[1].includes("if(V==='settings')return settingsPage();")) {
  throw new Error('Settings route does not use the customer update workflow');
}
if (match[1].includes('esp_ota_') || match[1].includes('FirmwareUpdateStateMachine')) {
  throw new Error('Browser payload contains platform update implementation details');
}

// Execute the visible local-update control in a small browser seam. This catches
// the failure mode where the chooser cannot select the signed package, leaving
// the visible button disabled and never reaching the controller request.
const localStart = match[1].indexOf('let updateSelectedFile=');
const localEnd = match[1].indexOf('let onlineOffer=', localStart);
if (localStart < 0 || localEnd < 0) throw new Error('local update workflow boundary missing');
const localUpdate = match[1].slice(localStart, localEnd);
const calls = [];
const xhrCalls = [];
const elements = {};
const app = {};
let renderCount = 0;
let scheduledTimeout = null;
Object.defineProperty(app, 'innerHTML', {
  set(value) {
    renderCount++;
    this.html = value;
    for (const id of ['updatePage', 'updatePanel', 'updateCurrentFirmware', 'updateControllerState', 'updateNotice',
      'updateFile', 'updateFileName', 'updateBegin', 'updateProgress', 'updateInstall', 'diagnosticsNav']) {
      elements[id] = elements[id] || {};
    }
    elements.updateFile.files = elements.updateFile.files || [];
    elements.updateFile.value = elements.updateFile.value || '';
    elements.updateBegin.disabled = /id="updateBegin"\s+disabled/.test(value);
  },
  get() { return this.html || ''; },
});
class FakeXhr {
  constructor() { this.upload = {}; }
  open(method, url) { this.method = method; this.url = url; }
  send() {
    xhrCalls.push(this.url);
    this.status = 202;
    this.responseText = '{"accepted":true}';
    queueMicrotask(() => this.onload());
  }
}
const browserContext = {
  console,
  window: { confirm: () => true },
  C: { isMaster: true },
  S: { lifecycle: 'READY' },
  A: app,
  V: 'settings',
  rememberView: () => {},
  $: (selector) => elements[selector.slice(1)] || null,
  G: async (path) => { calls.push(`GET ${path}`); return { state: 'IDLE' }; },
  fetch: async (url) => {
    calls.push(url);
    return { ok: true, status: url.endsWith('/begin') ? 202 : 200,
      json: async () => ({ accepted: true }) };
  },
  XMLHttpRequest: FakeXhr,
  queueMicrotask,
  setTimeout: (callback) => { scheduledTimeout = callback; return 1; },
  clearTimeout: () => { scheduledTimeout = null; },
};
vm.runInNewContext(`${localUpdate}\nthis.__runLocalUpdate = async () => {
  updateSettingsRender(null, {state:'IDLE'});
  const input = $('#updateFile');
  this.__initialInput = input;
  const button = $('#updateBegin');
  input.files = [{name:'candidate-B.ppu', size:1234}];
  input.onchange();
  if (button.disabled) throw new Error('signed package selection left UPDATE P&P disabled');
  await button.onclick();
};`, browserContext, {filename: `${payloadPath}#local-update-seam`});
browserContext.__runLocalUpdate().then(() => {
  if (calls[0] !== '/request/update/begin' || xhrCalls[0] !== '/request/update/upload' || calls[1] !== '/request/update/validate') {
    throw new Error(`visible UPDATE P&P control did not reach the expected request path: ${JSON.stringify({calls, xhrCalls})}`);
  }
  if (renderCount !== 1 || elements.updateFile !== browserContext.__initialInput) {
    throw new Error('firmware panel or file input was rebuilt during selection/upload/validation');
  }
  browserContext.updateSettingsRender(null, {state:'FAILED', validationFailure:'INVALID_CANDIDATE'});
  if (renderCount !== 1 || elements.updateFile !== browserContext.__initialInput) {
    throw new Error('status refresh rebuilt the firmware panel or file input');
  }
  const sameInput = elements.updateFile;
  sameInput.files = [{name:'replacement.ppu', size:2345}];
  sameInput.onchange();
  if (elements.updateFile !== sameInput || (elements.updateNotice.textContent || '').includes('could not validate')) {
    throw new Error('selecting a new file did not clear the previous validation error');
  }
  let installCalls = 0;
  browserContext.fetch = async (url) => {
    calls.push(url);
    if (url.endsWith('/install')) installCalls++;
    return { ok: true, status: 202, json: async () => ({accepted: true}) };
  };
  browserContext.updateSettingsRender({firmwareBuild:'known-good-A', buildEnvironment:'stage8bp1s3sd'}, {state:'CANDIDATE_READY'});
  const install = elements.updateInstall;
  if (install.hidden) throw new Error('candidate-ready install control was not offered');
  return Promise.all([install.onclick(), install.onclick()]).then(() => {
    if (installCalls !== 1) throw new Error(`duplicate install was not bounded: ${installCalls}`);
    if (!install.hidden) throw new Error('install control remained available while restart was pending');
    browserContext.updateSettingsRender({firmwareBuild:'candidate-B', buildEnvironment:'stage8bp1s3sd'}, {state:'IDLE', pendingReboot:false});
    if (!install.hidden || (elements.updateNotice.textContent || '').includes('Update complete')) {
      throw new Error('changed firmware identity without COMMITTED was treated as success');
    }
    if (typeof scheduledTimeout !== 'function') throw new Error('reconnect timeout was not armed');
    scheduledTimeout();
    if (!install.hidden || !(elements.updateNotice.textContent || '').includes('has not confirmed')) {
      throw new Error('unresolved reconnect released duplicate-install protection');
    }
    browserContext.updateSettingsRender({firmwareBuild:'candidate-B', buildEnvironment:'stage8bp1s3sd'}, {state:'COMMITTED', pendingReboot:false});
    if (!(elements.updateNotice.textContent || '').includes('Update complete') || (elements.updateCurrentFirmware.textContent || '').includes('Unknown')) {
      throw new Error('authoritative post-reconnect success was not presented');
    }
    browserContext.updateClearReconnectTimer();
  });
}).then(() => {
  console.log('WP6_BROWSER_UPDATE_UX_PASS');
}).catch((error) => {
  console.error(error.stack || error.message || error);
  process.exitCode = 1;
});
