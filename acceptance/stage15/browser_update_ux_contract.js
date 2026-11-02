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
  ['upload progress', 'Uploading…'],
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
  ['settled authority gate', 'Only the controlling Race Director can update P&amp;P while the race is settled.'],
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
  'INCOMPATIBLE_TARGET', 'The upload was interrupted. The current firmware is unchanged.']) {
  if (!match[1].includes(marker)) throw new Error(`customer update workflow missing ${marker}`);
}
if (!match[1].includes("const installAllowed=candidateReady&&canControl&&!updateUiBusy")) {
  throw new Error('install control is not gated by current authority/lifecycle and candidate readiness');
}
if (!match[1].includes('accept=".ppu,.bin,application/octet-stream"')) {
  throw new Error('local update file chooser does not accept signed .ppu packages');
}
if (!match[1].includes("${installAllowed?'<button class=\"btn primary\" id=\"updateInstall\">INSTALL AND RESTART</button>':''}")) {
  throw new Error('install control can be shown without the install-authority gate');
}
const installStart = match[1].indexOf('async function updateInstall(){');
const installEnd = match[1].indexOf('let onlineOffer=', installStart);
if (installStart < 0 || installEnd < 0) throw new Error('install action missing');
const install = match[1].slice(installStart, installEnd);
for (const marker of ["if(!C?.isMaster)", "if(!['READY','FINISHED'].includes(S?.lifecycle))", "if(!window.confirm('Install the validated update and restart P&P now?'))return", "/request/update/install"]) {
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
Object.defineProperty(app, 'innerHTML', {
  set(value) {
    this.html = value;
    for (const id of ['updateFile', 'updateBegin', 'updateProgress', 'updateInstall', 'diagnosticsNav']) {
      elements[id] = elements[id] || {};
    }
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
};
vm.runInNewContext(`${localUpdate}\nthis.__runLocalUpdate = async () => {
  updateSettingsRender(null, {state:'IDLE'});
  const input = $('#updateFile');
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
  console.log('WP6_BROWSER_UPDATE_UX_PASS');
}).catch((error) => {
  console.error(error.stack || error.message || error);
  process.exitCode = 1;
});
