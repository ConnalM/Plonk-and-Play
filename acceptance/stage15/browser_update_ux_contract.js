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
if (!match[1].includes("if(V==='settings')return settingsPage();")) {
  throw new Error('Settings route does not use the customer update workflow');
}
if (match[1].includes('esp_ota_') || match[1].includes('FirmwareUpdateStateMachine')) {
  throw new Error('Browser payload contains platform update implementation details');
}

console.log('WP6_BROWSER_UPDATE_UX_PASS');
