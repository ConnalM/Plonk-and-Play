'use strict';

// The shipped Browser relies on standard DOM APIs.  Node contract tests model
// only the APIs they exercise, and opt into these no-op host facilities here.
function completeBrowserDom(document) {
  if (!document.head) document.head = { appendChild() {} };
  if (!document.createElement) document.createElement = () => ({ textContent: '' });
  if (!document.querySelectorAll) document.querySelectorAll = () => [];
  return document;
}

module.exports = { completeBrowserDom };
