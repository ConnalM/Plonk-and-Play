from pathlib import Path
import argparse, re, sys
parser=argparse.ArgumentParser()
parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2])
args=parser.parse_args()
root=args.root.resolve()
files={name:(root/'firmware/include/pp'/name).read_text() for name in ['core.h','session_definition.h','race_engine.h','noticeboard.h','record_store.h','browser_interface.h']}
engine=files['race_engine.h']; session=files['session_definition.h']; notice=files['noticeboard.h']; records=files['record_store.h']; core=files['core.h']; browser=files['browser_interface.h']
checks={
 'shared_capacity':'PP_MAX_ENTRIES' in core and 'PP_MAX_ENTRIES' in engine and 'PP_MAX_ENTRIES' in session,
 'engine_not_two':'MaxEntries=PP_MAX_ENTRIES' in engine and 'observedMask_' not in engine and 'entryCount()==2' not in engine,
 'session_indexed':'entries[PP_MAX_ENTRIES]' in session and 'roles_[PP_MAX_ENTRIES]' in session and 'activeLanes>PP_MAX_ENTRIES' in session,
 'noticeboard_indexed':'entries[PP_MAX_ENTRIES]' in notice,
 'records_capacity':'pb[MaxEntries]' in records and 'lane>2' not in records,
 'input_capacity':'MaxDetectors=PP_MAX_ENTRIES' in core and 'detectors_[MaxDetectors]' in core,
 'browser_dynamic_records':"r.entries.map(e=>'Lane '+e.lane" in browser,
 'browser_dynamic_collections':('value.entryCount>PP_MAX_ENTRIES' in browser and 'const uint8_t count=' in browser and 'for(uint8_t i=0;i<r.entryCount' in browser and 'v.entries.map' in browser and 'r.entries.forEach' in browser),
 'no_new_practice':'Practice' not in ''.join(files.values()) and 'Endurance' not in ''.join(files.values()),
 'message_values_unchanged':'FinishSettlement=18' in core and 'FinishSettled=19' in core,
}
failed=[k for k,v in checks.items() if not v]
if failed: print('Stage 14A structural FAIL:',', '.join(failed));sys.exit(1)
print('Stage 14A structural PASS:',', '.join(checks))
