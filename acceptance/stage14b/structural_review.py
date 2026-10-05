from pathlib import Path
import argparse,sys
parser=argparse.ArgumentParser();parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2]);root=parser.parse_args().root.resolve()
core=(root/'firmware/include/pp/core.h').read_text()
session=(root/'firmware/include/pp/session_definition.h').read_text()
engine=(root/'firmware/include/pp/race_engine.h').read_text()
notice=(root/'firmware/include/pp/noticeboard.h').read_text()
browser=(root/'firmware/include/pp/browser_interface.h').read_text()
checks={
 'mode_contract':'SessionMode' in core and 'OpenPractice' in session,
 'generic_operations':'Resume, EndSession' in core,
 'indexed_entries':'PP_MAX_ENTRIES' in session and 'entries_[PP_MAX_ENTRIES]' in session and 'entries_[MaxEntries]' in engine,
 'practice_engine':'bool hasLap=false,waitingForTimingOrigin=false' in engine and 'OpenPractice' in engine and 'completedResult()const{return result_;}' in engine,
 'noticeboard_practice':'sessionMode' in notice and 'sessionFastestLap' in notice,
 'browser_routes':'/request/resume' in browser and '/request/end-session' in browser and 'OPEN_PRACTICE' in browser,
 'no_practice_records':'recordEligible_=definition_&&definition_->mode()==SessionMode::LapRace' in engine,
 'message_compatibility':'FinishSettlement=18' in core and 'FinishSettled=19' in core,
}
failed=[k for k,v in checks.items() if not v]
if failed: print('Stage 14B structural FAIL:',', '.join(failed));sys.exit(1)
print('Stage 14B structural PASS:', ', '.join(checks))
