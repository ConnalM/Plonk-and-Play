const fs = require('fs');
const vm = require('vm');

const root = process.cwd();
const pagePath = `${root}/firmware/browser/normal_browser.html`;
const embeddedPath = `${root}/firmware/include/pp/normal_browser_page.inc`;
const interfacePath = `${root}/firmware/include/pp/browser_interface.h`;
const page = fs.readFileSync(pagePath, 'utf8');
const embedded = fs.readFileSync(embeddedPath, 'utf8');
const interfaceSource = fs.readFileSync(interfacePath, 'utf8');

const start = page.indexOf('function systemHealthSummary(s)');
const end = page.indexOf('async function diagnosticsPage()', start);
if (start < 0 || end < 0) throw new Error('System Health formatter is missing');
const context = {};
vm.runInNewContext(`${page.slice(start, end)}; globalThis.format = systemHealthSummary;`, context);

function expect(label, value, parts) {
  for (const part of parts) {
    if (!value.includes(part)) throw new Error(`${label}: missing ${part}; got ${value}`);
  }
}

expect('healthy storage/history', context.format({physicalStorage: 'HEALTHY', historyHealth: 'HEALTHY', historyIndexed: 11, historyValid: 11}), [
  'Storage: Available', 'History: Healthy'
]);
expect('available storage with corrupt history', context.format({physicalStorage: 'MOUNTED', historyHealth: 'CORRUPT', historyIndexed: 11, historyValid: 9}), [
  'Storage: Available', 'History: Degraded — 9 of 11 records available'
]);
expect('unavailable storage', context.format({physicalStorage: 'UNAVAILABLE', historyHealth: 'CORRUPT', historyIndexed: 11, historyValid: 9}), [
  'Storage: Unavailable', 'History: Unavailable'
]);
expect('logical degradation does not relabel physical storage', context.format({physicalStorage: 'HEALTHY', historyHealth: 'CORRUPT', historyIndexed: 11, historyValid: 9}), [
  'Storage: Available'
]);

for (const marker of ['historyHealth', 'historyExcluded', 'historyIndexed', 'historyValid', 'physicalStorage', 'physicalDurable']) {
  if (!interfaceSource.includes(marker)) throw new Error(`diag/status missing ${marker}`);
}
if (!embedded.includes('function systemHealthSummary(s)') || !embedded.includes('Degraded — ${valid} of ${total} records available')) {
  throw new Error('embedded Browser does not contain the System Health formatter');
}
if (page.includes('Storage: ${s.storageHealth}')) throw new Error('legacy conflated Storage wording remains');

console.log('System Health storage/history wording: PASS');
