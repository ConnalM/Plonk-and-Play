from pathlib import Path
import re
import sys
import argparse

ROOT = Path(__file__).resolve().parents[2]
browser = (ROOT / "firmware/include/pp/browser_interface.h").read_text(encoding="utf-8")
normal = (ROOT / "firmware/browser/normal_browser.html").read_text(encoding="utf-8")
main = (ROOT / "firmware/src/main.cpp").read_text(encoding="utf-8")
notice = (ROOT / "firmware/include/pp/noticeboard.h").read_text(encoding="utf-8")
race_engine = (ROOT / "firmware/include/pp/race_engine.h").read_text(encoding="utf-8")
control = (ROOT / "firmware/include/pp/race_control.h").read_text(encoding="utf-8")
facilities = (ROOT / "firmware/tests/stage14c_facilities_probe.inc").read_text(encoding="utf-8")
proposal_runtime = (ROOT / "acceptance/stage14c/browser_proposal_runtime.js").read_text(encoding="utf-8")

parser = argparse.ArgumentParser(description="Review the production Stage 14C Browser path and final image payload.")
parser.add_argument("--demo-bin", type=Path, help="exact stage14cdemo merged image to inspect")
parser.add_argument("--acceptance-bin", type=Path, help="exact stage14cacceptance merged image to inspect")
args = parser.parse_args()

def image_contains(path: Path, text: bytes) -> bool:
    if not path.is_file():
        return False
    return text in path.read_bytes()

demo = args.demo_bin or (ROOT / "firmware/.pio/build/stage14cdemo/firmware.merged.bin")
acceptance = args.acceptance_bin or (ROOT / "firmware/.pio/build/stage14cacceptance/firmware.merged.bin")

checks = {
    "live_state_poll": "const state=await get('/state')" in browser and "$('#connection').textContent='synchronised'" in browser,
    "poll_state_not_revision_gated": bool(re.search(r"const n=await get\('/noticeboard'\).*?const state=await get\('/state'\)", browser, re.S)) and "if(changed){try{$('#fact')" in browser,
    "endurance_countdown": "displayRemaining" in browser and "clock(displayRemaining)" in browser and 'Overtime: '+"'+overtimeClock(v.overtime)+'" in browser,
    "overtime_clock_bounded": "const totalTenths=Math.max(0,Math.floor(Number(us||0)/100000))" in browser and "const totalSeconds=Math.floor(totalTenths/10)" in browser and "const seconds=totalSeconds-minutes*60" in browser and "const fraction=totalTenths-totalSeconds*10" in browser and "String(seconds).padStart(2,'0')" in browser,
    "authoritative_overtime": "s.finishBehaviour==LapFinishBehaviour::CompleteCurrentLap" in notice and "s.durationExpired" in notice,
    "browser_authority_gate": "window.browserHasMaster" in browser and "Race Director authority required to change the session proposal." in browser and "window.dispatchStart" in browser,
    "active_setup_reconstruction": "await get('/proposal')" in browser and "proposalRevision" in browser and "p.startable" in browser,
    "endurance_fixture_boundary": "mode!=pp::SessionMode::Endurance" in main and "input.setSimulatedSource" in main,
    "endurance_fixture_route_enabled": "defined(PP_STAGE14C_DEMO)" in browser and "defined(PP_STAGE14C_ACCEPTANCE)" in browser and '"/fixture", HTTP_POST, fixtureRoute' in browser,
    "fixture_route_calls_input_boundary": 'strstr(body,"lane1")&&instance()->fixturePass_' in browser and 'strstr(body,"lane2")&&instance()->fixturePass_' in browser and "input.setSimulatedSource" in main,
    "fixture_boundary_diagnostics": 'fixture_pass lane=%u disposition=SOURCE_ASSERTED' in main and 'reason=NO_ACTIVE_RACING_SESSION' in main and 'reason=INVALID_LANE' in main,
    "detector_labels": 'id="lane1">LANE 1 LAP' in browser and 'id="lane2">LANE 2 LAP' in browser,
    "demo_image_detector_labels": image_contains(demo, b"LANE 1 LAP") and image_contains(demo, b"LANE 2 LAP") and not image_contains(demo, b"LANE 1 PASS") and not image_contains(demo, b"LANE 2 PASS"),
    "acceptance_image_detector_labels": image_contains(acceptance, b"LANE 1 LAP") and image_contains(acceptance, b"LANE 2 LAP") and not image_contains(acceptance, b"LANE 1 PASS") and not image_contains(acceptance, b"LANE 2 PASS"),
    "demo_image_stage14c_controls": image_contains(demo, b"SESSION PROPOSAL") and image_contains(demo, b"SELECT OPEN PRACTICE") and image_contains(demo, b"SELECT ENDURANCE") and image_contains(demo, b"/request/setup"),
    "mode_selection_authority": "Race Director authority required to change the session proposal." in browser and "proposalRevision" in browser and "setupRoute" in browser,
    "mode_selection_exclusive": "selectPractice" in browser and "selectEndurance" in browser and "p.mode==='OPEN_PRACTICE'" in browser and "p.mode==='ENDURANCE'" in browser,
    "single_start_dispatcher": browser.count("document.querySelector('#start').onclick=") == 1 and "window.dispatchStart=()=>operation('/request/start','START')" in browser and "submitStart" in browser,
    "mode_proposal_preserved_in_ready": "await get('/proposal')" in browser and "window.reconcileModeFromState=()=>{}" in browser and "proposalRevision=Number(p.proposalRevision||0)" in browser,
    "mode_proposal_runtime_uses_production_poll": "baseScript" in proposal_runtime and "/proposal" in proposal_runtime and "/request/setup" in proposal_runtime and "vm.runInThisContext(authority" in proposal_runtime,
    "endurance_finish_policy_scope": '<option value="0">Stop at Zero</option><option value="1">Finish Current Lap</option>' in browser and 'Complete Full Race Distance' not in browser,
    "neutral_fixture_error": "An active session is required before triggering a simulated car." in browser,
    "browser_start_diagnostic_source": 'Browser server=%s error=%d' in main and 'browser.serverReady()||browser.serverStartError()' in main,
    "browser_start_diagnostic_demo_image": image_contains(demo, b"Browser server=%s error=%d"),
    "browser_wifi_recovery": "WiFi.reconnect()" in browser and "lastWifiAttemptMs_" in browser,
    "browser_wifi_monitored_after_start": "const bool connected = WiFi.status() == WL_CONNECTED" in browser and "if (server_) stopServer()" in browser,
    "browser_http_retired_on_wifi_loss": "httpd_stop(old)" in browser and "server_ = nullptr" in browser,
    "browser_recovery_observability": "wifiReconnectAttempts()" in browser and "Browser WiFi CONNECTED" in main and "Browser WiFi DISCONNECTED" in main and 'Browser HTTP server %s' in main,
    "browser_request_timeout": "AbortController" in browser and "requestTimeoutMs=4000" in browser and "polling=false" in browser,
    "browser_poll_overlap_guard": "if(polling)return" in browser and "setInterval(()=>poll(),250)" in browser,
    "normal_browser_request_timeout": "AbortController" in normal and "requestTimeoutMs=4000" in normal and "signal:controller.signal" in normal,
    "normal_browser_poll_overlap_guard": "async function poll(){if(polling)return;polling=true;" in normal and "finally{polling=false}" in normal and "setInterval(poll,1000)" in normal,
    "normal_browser_runtime_regression": "browser_normal_poll_runtime.js" in (ROOT / "acceptance/stage14c/run.ps1").read_text(encoding="utf-8"),
    "normal_browser_results_regression": "browser_results_runtime.js" in (ROOT / "acceptance/stage14c/run.ps1").read_text(encoding="utf-8") and "resultDataKey" in normal and "if(!r?.sealed)" in normal,
    "end_race_exposure_matches_authority": "${en?` <button class=\"btn primary\" id=\"endRace\">" in normal and "${pr?` <button class=\"btn primary\" id=\"end\">" not in normal,
    "endurance_default_duration_authority": "proposedRaceSetup.durationMinutes=10" in main,
    "browser_http_health_snapshot": "HTTP HEALTH" in main and "httpHealth(HttpHealth&" in browser and "else if(c=='h')browserHealth()" in main,
    "browser_route_trace": "RequestTrace trace(instance(), \"/state\")" in browser and "httpActiveHandlers_" in browser and "httpLastRoute_" in browser,
    "browser_heap_and_error_observability": "ESP.getMinFreeHeap()" in browser and "ESP.getMinFreeHeap()" in main and "httpRequestErrors_" in browser,
    "latest_lap_is_authoritative_cross_entry": "entry.recordCount&&entry.records[entry.recordCount-1].finishTime" in notice and "selectLatestLap" in notice,
    "authoritative_live_position": "livePosition" in race_engine and "position" in notice and '\\"position\\"' in browser,
    "authoritative_live_gap": "liveLapsBehind" in race_engine and "lapsBehind" in notice and '\\"lapsBehind\\"' in browser,
    "position_preserves_physical_lane": "const uint8_t lane=control_.definition()?control_.definition()->entry(i).lane" in notice,
    "authoritative_start_light_step": "redLightsLit(Time now)" in control and "redLightsLit" in notice and '\\"redLightsLit\\"' in browser,
    "presentation_boundary_fixtures": all(token in facilities for token in ("14C.P7", "14C.P8", "14C.P9", "14C.P10", "redLightsLit")),
}
failed = [name for name, ok in checks.items() if not ok]
if failed:
    print("Stage 14C Browser integration review FAIL:", ", ".join(failed))
    sys.exit(1)
print("Stage 14C Browser integration review PASS:", ", ".join(checks))
