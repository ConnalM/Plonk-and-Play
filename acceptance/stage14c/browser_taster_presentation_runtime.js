'use strict';

// Exercises the actual C1 Browser extension with a deliberately small DOM.
// Polling, reconnection and authority changes must not overwrite an editor's
// draft; only Cancel or an accepted mutation may discard it.
const fs = require('fs');
const vm = require('vm');

const html = fs.readFileSync('firmware/browser/normal_browser.html', 'utf8');
const finalScript = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].at(-1)[1];
const marker = /\(function\(\)\{\s*const c1BaseRender=window\.render;/g;
let match, start = -1;
while ((match = marker.exec(finalScript))) start = match.index;
if (start < 0) throw new Error('C1 Browser extension not found');
const extension = finalScript.slice(start);
const tick = () => new Promise(resolve => setTimeout(resolve, 0));

function browserHarness() {
  const elements = {};
  const document = { activeElement: null };
  const install = markup => {
    for (const match of markup.matchAll(/<[a-z]+[^>]*\bid="([^"]+)"[^>]*>/gi)) {
      const tag = match[0], id = match[1];
      elements[id] = {
        id, value: '', disabled: /\sdisabled(?:\s|>)/.test(tag), selectionStart: 0, selectionEnd: 0,
        focus() { document.activeElement = this; },
      };
    }
  };
  const A = {};
  Object.defineProperty(A, 'innerHTML', {
    get() { return this._html || ''; },
    set(value) { this._html = value; Object.keys(elements).forEach(key => delete elements[key]); install(value); },
  });
  let committed = { lane1Label: 'LANE 1', lane2Label: 'LANE 2', lane1Graphic: 0, lane2Graphic: 1 };
  let failSave = false, requests = 0, posts = [];
  const context = {
    A, document, P: {}, S: { lifecycle: 'READY' }, C: { isMaster: true }, V: 'settings',
    window: { render() {}, confirm() { return true; } }, header() {},
    $(selector) { return elements[selector.slice(1)] || null; },
    G: async path => {
      if (path !== '/taster/preferences') throw new Error('unexpected GET ' + path);
      ++requests;
      return { ...committed };
    },
    fetch: async (path, options) => {
      posts.push({ path, body: JSON.parse(options.body) });
      if (failSave) throw new Error('offline');
      if (path.endsWith('/restore-factory')) committed = { lane1Label: 'LANE 1', lane2Label: 'LANE 2', lane1Graphic: 0, lane2Graphic: 1 };
      else committed = { ...JSON.parse(options.body) };
      return { json: async () => ({ accepted: true }) };
    },
    Number, String, Object, Array, setTimeout, clearTimeout,
  };
  vm.runInNewContext(extension, context, { filename: 'taster_presentation_browser_extension.js' });
  return { context, A, elements, get committed() { return committed; }, get requests() { return requests; }, get posts() { return posts; }, set failSave(value) { failSave = value; } };
}

(async () => {
  const app = browserHarness();
  app.context.window.render(); await tick(); await tick();
  const initialRequests = app.requests;
  if (app.elements.c1Lane1Label.value !== 'LANE 1') throw new Error('initial preferences did not load');

  // A slow editor survives ordinary Browser refreshes, a temporary loss of
  // authority and reconnection without a second read clobbering the draft.
  app.elements.c1Lane1Label.value = 'The slow editor'; app.elements.c1Lane1Label.selectionStart = app.elements.c1Lane1Label.selectionEnd = 4; app.elements.c1Lane1Label.focus(); app.elements.c1Lane1Label.oninput();
  app.context.window.render();
  if (app.elements.c1Lane1Label.value !== 'The slow editor' || app.requests !== initialRequests || app.context.document.activeElement !== app.elements.c1Lane1Label || app.elements.c1Lane1Label.selectionStart !== 4) throw new Error('poll refresh overwrote draft or disrupted caret');
  app.context.C.isMaster = false; app.context.window.render();
  if (app.elements.c1Lane1Label.value !== 'The slow editor' || !app.elements.c1Save.disabled) throw new Error('authority loss did not preserve and lock draft');
  app.context.C.isMaster = true; app.context.window.render();
  if (app.elements.c1Lane1Label.value !== 'The slow editor' || app.elements.c1Save.disabled) throw new Error('reconnection did not retain draft');

  // A failed Save preserves what the user typed. An accepted Save is the
  // deliberate boundary at which the local draft is replaced from firmware.
  app.failSave = true; app.elements.c1Save.onclick(); await tick(); await tick();
  if (app.elements.c1Lane1Label.value !== 'The slow editor' || !app.elements.c1Message.innerHTML.includes('could not be saved')) throw new Error('failed save discarded draft');
  app.failSave = false; app.elements.c1Save.onclick(); await tick(); await tick();
  if (app.committed.lane1Label !== 'The slow editor' || app.elements.c1Lane1Label.value !== 'The slow editor') throw new Error('accepted save did not commit/reload draft');

  // Factory restore remains explicitly confirmed and only an accepted action
  // replaces the draft. Cancel is the other explicit discard path.
  app.elements.c1Lane2Label.value = 'Unsaved'; app.elements.c1Lane2Label.oninput();
  app.elements.c1Cancel.onclick(); await tick(); await tick();
  if (app.elements.c1Lane2Label.value !== app.committed.lane2Label) throw new Error('Cancel did not discard draft');
  app.elements.c1Restore.onclick(); await tick(); await tick();
  if (app.committed.lane1Label !== 'LANE 1' || !app.posts.some(post => post.path.endsWith('/restore-factory'))) throw new Error('factory restore was not confirmed and applied');
  console.log('Taster presentation Browser runtime PASS: slow edit, poll, authority loss/reconnect, failed save, cancel and factory restore');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
