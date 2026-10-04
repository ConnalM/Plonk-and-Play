"""Structural review for frozen Stage 10 authority and isolation boundaries."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent / 'evidence'
browser = (ROOT / 'firmware/include/pp/browser_interface.h').read_text(encoding='utf-8')
control = (ROOT / 'firmware/include/pp/race_control.h').read_text(encoding='utf-8')
engine = (ROOT / 'firmware/include/pp/race_engine.h').read_text(encoding='utf-8')
notice = (ROOT / 'firmware/include/pp/noticeboard.h').read_text(encoding='utf-8')
session = (ROOT / 'firmware/include/pp/session_definition.h').read_text(encoding='utf-8')
core = (ROOT / 'firmware/include/pp/core.h').read_text(encoding='utf-8')
main = (ROOT / 'firmware/src/main.cpp').read_text(encoding='utf-8')

start = session.index('class SessionDefinition')
end = session.index('};', start) + 2
session_definition = session[start:end]
checks = [
 ('Browser Interface is the sole Browser-side Bus participant', 'Bus& bus_' in browser and 'bus_.publish(endpoint_, request)' in browser and 'BrowserInterface browser' in main),
 ('Cookie identity contains no role or authority claim', 'pp_browser=' in browser and 'Race Director' not in browser[browser.index('setCookie'):browser.index('static uint64_t client')] and 'fingerprint(token)' in browser),
 ('Retained Master binding is Browser Interface state only', 'AuthorityStore' in browser and 'BrowserAuthorityStore' in main and 'pp-browser-auth' in main and 'masterFingerprint_' in browser),
 ('Race Control has no Browser identity/token/cookie dependency', 'cookie' not in control.lower() and 'token' not in control.lower() and 'ClientContext' in control),
 ('Race Engine and Noticeboard have no Browser authority ownership', 'BrowserInterface' not in engine and 'cookie' not in engine.lower() and 'BrowserInterface' not in notice and 'cookie' not in notice.lower()),
 ('Session Definition contains no Browser identity', 'cookie' not in session_definition.lower() and 'Browser' not in session_definition),
 ('START authority remains Message Bus restricted', 'case Type::StartRequest: return mask(Role::Presentation)' in core and 'case Type::StartRequest: return mask(Role::RaceControl)' in core),
 ('Request Results remain Race Control to Presentation only', 'case Type::RequestResult: return mask(Role::RaceControl)' in core and 'case Type::RequestResult: return mask(Role::Presentation)' in core),
 ('Browser state is current Noticeboard state, not replay', 'return noticeboard_.current()' in browser and '/replay' not in browser),
 ('Presentation loss cannot block Input/Race Engine', 'presentationBestEffort(m.type)' in core and 'Role::Presentation' in core and 'Role::Input' in core),
 # Stage 12+ Browser formatting may name the immutable completed-result value
 # type for Results/History reconstruction.  It must still have no Engine or
 # Control collaborator and no direct lifecycle/session operation.
 ('No private Browser-to-Race-Control control route', 'RaceControlModule' not in browser and 'RaceEngineModule&' not in browser and 'RaceEngineModule*' not in browser and 'active_.commit' not in browser),
]
findings = [{'requirement': name, 'result': 'PASS' if ok else 'FAIL'} for name, ok in checks]
payload = {'frozen_acceptance_commit': 'ba194fed214a638098613e1318c703a650c3e228', 'findings': findings}
(OUT / 'structural-review.json').write_text(json.dumps(payload, indent=2) + '\n', encoding='utf-8')
(OUT / 'structural-review.md').write_text('# Stage 10 structural review\n\n' + '\n'.join(f"- **{x['result']}** — {x['requirement']}" for x in findings) + '\n', encoding='utf-8')
for item in findings:
 print(item['requirement'], item['result'])
raise SystemExit(0 if all(x['result'] == 'PASS' for x in findings) else 1)
