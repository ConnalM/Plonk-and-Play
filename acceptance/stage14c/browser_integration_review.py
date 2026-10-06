from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
browser = (ROOT / "firmware/include/pp/browser_interface.h").read_text(encoding="utf-8")
main = (ROOT / "firmware/src/main.cpp").read_text(encoding="utf-8")
notice = (ROOT / "firmware/include/pp/noticeboard.h").read_text(encoding="utf-8")

checks = {
    "live_state_poll": "const state=await get('/state')" in browser and "$('#connection').textContent='synchronised'" in browser,
    "poll_state_not_revision_gated": bool(re.search(r"const n=await get\('/noticeboard'\).*?const state=await get\('/state'\)", browser, re.S)) and "if(changed){try{$('#fact')" in browser,
    "endurance_countdown": 'Time remaining: '+"'+clock(v.remainingDuration)+'" in browser and 'Overtime: '+"'+overtimeClock(v.overtime)+'" in browser,
    "authoritative_overtime": "s.finishBehaviour==LapFinishBehaviour::CompleteCurrentLap" in notice and "s.durationExpired" in notice,
    "browser_authority_gate": "window.browserHasMaster" in browser and "Race Director authority required to start Endurance." in browser,
    "active_setup_reconstruction": "minutes.value=String(v.durationMinutes)" in browser and "v.finishBehaviour==='COMPLETE_CURRENT_LAP'" in browser,
    "endurance_fixture_boundary": "mode!=pp::SessionMode::Endurance" in main and "input.setSimulatedSource" in main,
    "endurance_fixture_route_enabled": "defined(PP_STAGE14C_DEMO)" in browser and "defined(PP_STAGE14C_ACCEPTANCE)" in browser and '"/fixture", HTTP_POST, fixtureRoute' in browser,
    "fixture_route_calls_input_boundary": 'strstr(body,"lane1")&&instance()->fixturePass_' in browser and 'strstr(body,"lane2")&&instance()->fixturePass_' in browser and "input.setSimulatedSource" in main,
    "detector_labels": 'id="lane1">LANE 1 LAP' in browser and 'id="lane2">LANE 2 LAP' in browser,
    "mode_selection_authority": "Race Director authority required to select Open Practice." in browser and "window.browserHasMaster!==true||!!v&&v.lifecycle!=='READY'" in browser,
    "mode_selection_exclusive": "window.clearPracticeSelection" in browser and "window.clearEnduranceSelection" in browser,
    "neutral_fixture_error": "An active session is required before triggering a simulated car." in browser,
}
failed = [name for name, ok in checks.items() if not ok]
if failed:
    print("Stage 14C Browser integration review FAIL:", ", ".join(failed))
    sys.exit(1)
print("Stage 14C Browser integration review PASS:", ", ".join(checks))
