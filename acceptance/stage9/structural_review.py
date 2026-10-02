import json
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=Path(__file__).parent/'evidence';core=(root/'firmware/include/pp/core.h').read_text();engine=(root/'firmware/include/pp/race_engine.h').read_text();control=(root/'firmware/include/pp/race_control.h').read_text();session=(root/'firmware/include/pp/session_definition.h').read_text();input_module=core[core.index('class InputModule'):core.index('struct OutputModule')]
checks={
'Input Module owns bounded protected backlog':'ProtectedDepth=8' in input_module and 'backlog_' in input_module,
'Input Module publishes standard INPUT_EVENT':'e.type=Type::InputEvent' in input_module and 'bus_.publish' in input_module,
'No direct Input Module to Race Engine call':'RaceEngineModule' not in input_module,
'Race Engine owns per-entry state':'EntryState entries_[2]' in engine,
'Relevant Time ordering is explicit':'pending_[j].relevantTime>x.relevantTime' in engine,
'Fault remains a Bus message with authority':'RaceIntegrityFault' in core and 'case Type::RaceIntegrityFault: return mask(Role::Input)' in core,
'Fault priority is bounded Bus queue policy':'m.type==Type::RaceIntegrityFault&&s.role==Role::RaceEngine' in core,
'Race Control owns fault consequence':'integrityFaulted_' in control and 'SessionLifecycle::Faulted' in control,
'Session Definition holds two lane roles and entries':'roles_[2]' in session and 'entries_[2]' in session,
}
findings=[{'requirement':k,'result':'PASS' if v else 'FAIL'} for k,v in checks.items()];(out/'structural-review.json').write_text(json.dumps({'findings':findings},indent=2)+'\n');print('\n'.join(f"{x['result']} {x['requirement']}" for x in findings));raise SystemExit(0 if all(x['result']=='PASS' for x in findings) else 1)
