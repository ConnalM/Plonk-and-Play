from pathlib import Path

import json, re

ROOT=Path(__file__).resolve().parents[2]

core=(ROOT/'firmware/include/pp/core.h').read_text()

engine=(ROOT/'firmware/include/pp/race_engine.h').read_text()

control=(ROOT/'firmware/include/pp/race_control.h').read_text()

browser=(ROOT/'firmware/include/pp/browser_interface.h').read_text()

input_code=core

checks={

 'types_append_18_19': 'FinishSettlement=18, FinishSettled=19, Count=20' in core,

 'finish_publish_authority': 'case Type::FinishSettlement: return mask(Role::RaceEngine);' in core,

 'finish_reply_authority': 'case Type::FinishSettled: return mask(Role::Input);' in core,

 'normal_bus_routes': all(x in core for x in ['case Type::FinishSettlement:', 'case Type::FinishSettled:']),

 'race_engine_owns_result': 'struct CompletedRaceResult' in engine and 'void seal()' in engine,

 'control_does_not_rank': all(x not in control for x in ['fastestLap','lapsBehind','void seal()']),

 # Stage 13 can select an immutable persisted completed result after a
 # controller reboot.  That is reconstruction of Race Engine-owned data, not
 # Browser calculation of W/F, rank, or lap records.
 'browser_does_not_calculate_result': 'displayResult' in browser and 'noticeboard_.completedResult()' in browser and 'void seal()' not in browser and 'applyFinish' not in browser,

 'input_does_not_interpret_finish_rules': 'finishBehaviour_' not in input_code and 'lapTarget()' not in input_code,

 'browser_not_bus_participant': 'bus.attach' not in browser and 'bus.subscribe' not in browser,

 'details_use_retained_laps': all(x in browser for x in ['e.records[j]','lap.lapTime','lap.startTime','lap.finishTime']),

 'race_again_authority': 'ClientContext::RaceDirectorSmug' in control and 'LifecycleNotRaceAgain' in control and '/request/race-again' in browser,

 'human_result_fallbacks': 'No valid completed laps.' in browser and 'fastestLap?sec' in browser,

 'home_has_no_server_route': '"/request/home"' not in browser,

}

print(json.dumps(checks,indent=2))

if not all(checks.values()):raise SystemExit(1)

print('Stage 12 structural review PASS')

