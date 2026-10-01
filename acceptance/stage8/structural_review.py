"""Static ownership and isolation review required by frozen Stage 8."""
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
OUT=Path(__file__).resolve().parent/'evidence'
control=(ROOT/'firmware/include/pp/race_control.h').read_text(encoding='utf-8')
engine=(ROOT/'firmware/include/pp/race_engine.h').read_text(encoding='utf-8')
notice=(ROOT/'firmware/include/pp/noticeboard.h').read_text(encoding='utf-8')
browser=(ROOT/'firmware/include/pp/browser_interface.h').read_text(encoding='utf-8')
session=(ROOT/'firmware/include/pp/session_definition.h').read_text(encoding='utf-8')
core=(ROOT/'firmware/include/pp/core.h').read_text(encoding='utf-8')
start=session.index('class SessionDefinition')
end=session.index('};',start)+2
session_type=session[start:end]
checks=[
 ('Race Control consumes and decides START_REQUEST', 'Type::StartRequest' in control and 'handleStart' in control and 'result(request.correlation' in control),
 ('Race Engine does not consume START_REQUEST or commit sessions', 'StartRequest' not in engine and '.commit(' not in engine),
 ('Noticeboard does not decide START_REQUEST', 'StartRequest' not in notice),
 ('Race Control owns session commit and existing start procedure', 'active_.commit' in control and 'beginStart' in control and 'Type::GoScheduled' in control),
 ('Session Definition is fixed data, not a Bus participant', 'Bus' not in session_type and 'commit(' not in session_type),
 ('Browser Interface is the Bus participant', 'Bus& bus' in browser and 'bus_.publish' in browser),
 ('Browser has no direct Race Control or Race Engine control route', 'RaceControlModule' not in browser and 'RaceEngineModule' not in browser and 'request.type=Type::GoScheduled' not in browser),
 ('Bus authority restricts START_REQUEST and REQUEST_RESULT', 'case Type::StartRequest: return mask(Role::Presentation)' in core and 'case Type::RequestResult: return mask(Role::RaceControl)' in core),
 ('Presentation loss policy remains message-contract-specific', 'presentationBestEffort(m.type)' in core and 'case Type::RequestResult' not in core[core.index('static bool presentationBestEffort'):core.index('static uint16_t publishers')]),
 ('Browser State is derived from Noticeboard cache', 'const Noticeboard& noticeboard_' in browser and 'return noticeboard_.current()' in browser),
]
findings=[{'requirement':name,'result':'PASS' if ok else 'FAIL'} for name,ok in checks]
payload={'frozen_acceptance_commit':'20469948784a64662ef96cb20637cfd6a59cc27b','findings':findings}
(OUT/'structural-review.json').write_text(json.dumps(payload,indent=2)+'\n',encoding='utf-8')
(OUT/'structural-review.md').write_text('# Stage 8 structural review\n\n'+'\n'.join(f"- **{item['result']}** — {item['requirement']}" for item in findings)+'\n',encoding='utf-8')
for item in findings: print(item['requirement'], item['result'])
raise SystemExit(0 if all(item['result']=='PASS' for item in findings) else 1)
