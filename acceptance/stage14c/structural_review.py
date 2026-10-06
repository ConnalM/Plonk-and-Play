from pathlib import Path
import argparse,sys
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2]);r=p.parse_args().root.resolve();core=(r/'firmware/include/pp/core.h').read_text();session=(r/'firmware/include/pp/session_definition.h').read_text();engine=(r/'firmware/include/pp/race_engine.h').read_text();notice=(r/'firmware/include/pp/noticeboard.h').read_text();browser=(r/'firmware/include/pp/browser_interface.h').read_text()
checks={'endurance_mode':'Endurance' in core and 'durationMinutes' in session,'expiry_contract':'EnduranceExpired' in core and 'durationExpiryAt' in notice,'lap_penalty':'lapPenalty' in engine and 'classifiedLaps' in engine,'relevant_time':'relevantTime' in engine and 'expiryTime' in engine,'generic_ops':'Resume, EndSession' in core,'browser_endurance':'ENDURANCE' in browser and 'durationMinutes' in browser,'official_result':'CompletedRaceResult' in engine and 'HistoryStored' in core,'no_practice_records':'OpenPractice' in engine and 'recordEligible_' in engine,'format_version':'ResultFormatVersion=3' in engine}
bad=[k for k,v in checks.items() if not v]
if bad:print('Stage 14C structural FAIL:',', '.join(bad));sys.exit(1)
print('Stage 14C structural PASS:',', '.join(checks))
