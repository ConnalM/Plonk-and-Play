const fs = require('fs');
const vm = require('vm');

const payloadPath = process.argv[2] || 'firmware/include/pp/normal_browser_page.inc';
const text = fs.readFileSync(payloadPath, 'utf8');
const start = text.indexOf('R"PPHTML(');
const end = text.lastIndexOf(')PPHTML"');
if (start < 0 || end <= start) throw new Error('embedded Browser payload missing');
const html = text.slice(start + 'R"PPHTML('.length, end);
const scripts = [...html.matchAll(/<script(?: [^>]*)?>([\s\S]*?)<\/script>/gi)];
if (!scripts.length) throw new Error('embedded Browser script missing');
for (const [index, match] of scripts.entries()) {
  new vm.Script(match[1], { filename: `racer-browser-${index + 1}.js` });
}
console.log('Generated Racer Browser JavaScript parse: PASS');
