from pathlib import Path
import re
import sys
import argparse

ROOT = Path(__file__).resolve().parents[2]
browser = (ROOT / "firmware/include/pp/browser_interface.h").read_text(encoding="utf-8")
normal = (ROOT / "firmware/browser/normal_browser.html").read_text(encoding="utf-8")
embedded_normal = (ROOT / "firmware/include/pp/normal_browser_page.inc").read_text(encoding="utf-8")
main = (ROOT / "firmware/src/main.cpp").read_text(encoding="utf-8")
notice = (ROOT / "firmware/include/pp/noticeboard.h").read_text(encoding="utf-8")
race_engine = (ROOT / "firmware/include/pp/race_engine.h").read_text(encoding="utf-8")
control = (ROOT / "firmware/include/pp/race_control.h").read_text(encoding="utf-8")
facilities = (ROOT / "firmware/tests/stage14c_facilities_probe.inc").read_text(encoding="utf-8")
proposal_runtime = (ROOT / "acceptance/stage14c/browser_proposal_runtime.js").read_text(encoding="utf-8")
setup_runtime = (ROOT / "acceptance/stage14c/browser_setup_sync_runtime.js").read_text(encoding="utf-8")
run_script = (ROOT / "acceptance/stage14c/run.ps1").read_text(encoding="utf-8")

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
    "resume_countdown_uses_remaining": "phase==='RESUME_COUNTDOWN'" in normal and "S.remainingDuration" in normal and "durationMinutes" in normal,
    "overtime_clock_bounded": "const totalTenths=Math.max(0,Math.floor(Number(us||0)/100000))" in browser and "const totalSeconds=Math.floor(totalTenths/10)" in browser and "const seconds=totalSeconds-minutes*60" in browser and "const fraction=totalTenths-totalSeconds*10" in browser and "String(seconds).padStart(2,'0')" in browser,
    "authoritative_overtime": "s.finishBehaviour==LapFinishBehaviour::CompleteCurrentLap" in notice and "s.durationExpired" in notice,
    "browser_authority_gate": "window.browserHasMaster" in browser and "Race Director authority required to change the session proposal." in browser and "window.dispatchStart" in browser,
    "active_setup_reconstruction": "await get('/proposal')" in browser and "proposalRevision" in browser and "p.startable" in browser,
    "proposal_controller_epoch": "controllerEpoch" in browser and "controllerEpoch_" in browser and "newControllerEpoch" in browser,
    "proposal_epoch_runtime_regression": "epoch-a" in setup_runtime and "old-epoch response" in setup_runtime and "same-epoch older" in setup_runtime,
    "endurance_fixture_boundary": "mode!=pp::SessionMode::Endurance" in main and "input.setSimulatedSource" in main,
    "endurance_fixture_route_enabled": "defined(PP_STAGE14C_DEMO)" in browser and "defined(PP_STAGE14C_ACCEPTANCE)" in browser and '"/fixture", HTTP_POST, fixtureRoute' in browser,
    "fixture_route_calls_input_boundary": 'strstr(body,"lane1")&&instance()->fixturePass_' in browser and 'strstr(body,"lane2")&&instance()->fixturePass_' in browser and "input.setSimulatedSource" in main,
    "fixture_boundary_diagnostics": 'fixture_pass lane=%u disposition=SOURCE_ASSERTED' in main and 'reason=NO_ACTIVE_RACING_SESSION' in main and 'reason=INVALID_LANE' in main,
    "detector_labels": 'id="lane1">LANE 1 LAP' in browser and 'id="lane2">LANE 2 LAP' in browser,
    # The current diagnostic fixture is intentionally identified by its
    # request boundary and lane actions.  The old checks looked for literal
    # page labels from a retired diagnostic-page layout and failed even when
    # the current fixture route was embedded and callable.
    "demo_image_detector_labels": image_contains(demo, b"/fixture") and image_contains(demo, b"lane1") and image_contains(demo, b"lane2") and not image_contains(demo, b"LANE 1 PASS"),
    "acceptance_image_detector_labels": image_contains(acceptance, b"/fixture") and image_contains(acceptance, b"lane1") and image_contains(acceptance, b"lane2") and not image_contains(acceptance, b"LANE 1 PASS"),
    "demo_image_stage14c_controls": image_contains(demo, b"/request/setup") and image_contains(demo, b"/fixture") and image_contains(demo, b"lane1") and image_contains(demo, b"lane2"),
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
    "normal_browser_poll_overlap_guard": "async function poll(" in normal and "if(polling){pollDirty=true;return}" in normal and "finally{polling=false" in normal and "setInterval(poll,1000)" not in normal,
    "normal_browser_sse_notification": "new EventSource('/events')" in normal and "openEventStream" in normal and "window.poll" in normal,
    "embedded_browser_sse_notification": "new EventSource('/events')" in embedded_normal,
    "normal_browser_sse_liveness": "eventStreamLivenessTimeoutMs" in embedded_normal and "liveness_timeout" in embedded_normal and "addEventListener('heartbeat'" in embedded_normal,
    "normal_browser_sse_liveness_runtime": "browser_sse_liveness_runtime.js" in (ROOT / "acceptance/stage14c/run.ps1").read_text(encoding="utf-8"),
    "normal_browser_sse_admission_recovery_runtime": "browser_sse_admission_recovery_runtime.js" in (ROOT / "acceptance/stage14c/run.ps1").read_text(encoding="utf-8"),
    "normal_browser_reboot_recovery_runtime": "browser_reboot_recovery_runtime.js" in (ROOT / "acceptance/stage14c/run.ps1").read_text(encoding="utf-8"),
    "normal_browser_sequential_recovery_marker": "const recoveryAcquisition=trigger==='reconnect'||trigger==='recovery'" in embedded_normal,
    "http_sse_notification_boundary": '"/events", HTTP_GET, events, nullptr' in browser and "struct SseSession" in browser and "struct PageJob" in browser and "httpd_req_async_handler_begin" in browser and "httpd_req_async_handler_complete" in browser and "httpd_resp_send_chunk(session->asyncRequest" in browser and "freeSseSession" in browser and "httpd_sess_get_ctx" in browser and "session->notifyPending" in browser and "queuedGeneration" in browser and "session->sentRevision" in browser and "event: heartbeat" in browser and "PageWorkerCount = 2" in browser and "PP_HTTP_ORDINARY_REQUEST_RESERVE" in browser and "serverGeneration" in browser and "httpd_queue_work(server_, &BrowserInterface::eventWork, work)" not in browser and "httpd_socket_send" not in browser,
    "notification_runtime_regression": "browser_notification_runtime.js" in (ROOT / "acceptance/stage14c/run.ps1").read_text(encoding="utf-8"),
    "normal_browser_runtime_regression": "browser_normal_poll_runtime.js" in (ROOT / "acceptance/stage14c/run.ps1").read_text(encoding="utf-8"),
    "normal_browser_setup_sync": "setupDesired" in normal and "setupRunning" in normal and "waitSetupResult" in normal and "browser_setup_sync_runtime.js" in (ROOT / "acceptance/stage14c/run.ps1").read_text(encoding="utf-8") and "rapid coalescing" in setup_runtime and "stale proposal response" in setup_runtime,
    "normal_browser_results_regression": "browser_results_runtime.js" in (ROOT / "acceptance/stage14c/run.ps1").read_text(encoding="utf-8") and "resultDataKey" in normal and "if(!r?.sealed)" in normal,
    "authoritative_finish_duration_boundary": "finishDurationUs" in browser and "finishDurationUs" in normal,
    "normal_browser_resume_regression": "browser_resume_runtime.js" in run_script,
    "final_binary_provenance_regression": "browser_binary_provenance.py" in run_script,
    "termination_controls_follow_lifecycle": "id=\"endRace\"" not in normal and "id=\"abandon\"" in normal and "id=\"endPractice\"" in normal and "/request/end-session" in normal,
    "endurance_default_duration_authority": "proposedRaceSetup.durationMinutes=10" in main,
    "browser_http_health_snapshot": "HTTP HEALTH" in main and "httpHealth(HttpHealth&" in browser and "else if(c=='h')browserHealth()" in main,
    "browser_route_trace": "RequestTrace trace(instance(), \"/state\")" in browser and "httpActiveHandlers_" in browser and "httpLastRoute_" in browser,
    "browser_heap_and_error_observability": "ESP.getMinFreeHeap()" in browser and "ESP.getMinFreeHeap()" in main and "httpRequestErrors_" in browser,
    # The noticeboard must use the authoritative per-entry crossing summary;
    # dereferencing entry.records here can crash when detail is streamed or
    # resident only in the persistence boundary.
    "latest_lap_is_authoritative_cross_entry": "selectLatestLap" in notice and "entry.lastCrossing>=latest" in notice and "entry.recordCount&&entry.records[entry.recordCount-1].finishTime" not in notice,
    # Details has no streamed-detail source.  A non-zero record count without
    # resident records must therefore produce a bounded unavailable response,
    # never enter the record loop and never crash the HTTPD task.
    "details_null_records_guard": "detail unavailable" in browser and "HTTPD_500_INTERNAL_SERVER_ERROR" in browser and "if(e.recordCount&&e.records==nullptr)" in browser,
    "authoritative_live_position": "livePosition" in race_engine and "position" in notice and '\\"position\\"' in browser,
    "authoritative_live_gap": "liveLapsBehind" in race_engine and "lapsBehind" in notice and '\\"lapsBehind\\"' in browser,
    "position_preserves_physical_lane": "const uint8_t lane=control_.definition()?control_.definition()->entry(i).lane" in notice,
    "authoritative_start_light_step": "redLightsLit(Time now)" in control and "redLightsLit" in notice and '\\"redLightsLit\\"' in browser,
    "authoritative_start_schedule_time_basis": "S.startPresentation.redLightsLit" in normal and "S.startPresentation.phase" in normal and "schedulePresentation" not in normal and "pTimeOffsetUs" not in normal,
    "p_and_p_timer_invalidations": "presentationAt(Time now)" in notice and "updatePresentation(Time now)" in browser and "updateTimerInvalidation(Time now)" not in browser and "timerDisplaySecond_" not in browser,
    "no_connected_browser_timer_poll": "setInterval(poll,1000)" not in normal,
    "presentation_boundary_fixtures": all(token in facilities for token in ("14C.P7", "14C.P8", "14C.P9", "14C.P10", "redLightsLit")),
}

recovery_marker = "const recoveryAcquisition=trigger==='reconnect'||trigger==='recovery'"
recovery_start = embedded_normal.index(recovery_marker)
recovery_branch_start = embedded_normal.index("if(recoveryAcquisition){", recovery_start)
recovery_branch_end = embedded_normal.index("}else if(activeBefore)", recovery_branch_start)
recovery_branch = embedded_normal[recovery_branch_start:recovery_branch_end]
checks["normal_browser_sequential_recovery_order"] = all(
    recovery_branch.find(token) >= 0
    for token in (
        "timedS=await readTimedState",
        "nextP=await tracedGet('/proposal'",
        "nextC=await tracedGet('/context'",
    )
)
checks["normal_browser_recovery_has_no_concurrent_batch"] = "Promise.all" not in recovery_branch
failed = [name for name, ok in checks.items() if not ok]
if failed:
    print("Stage 14C Browser integration review FAIL:", ", ".join(failed))
    sys.exit(1)
print("Stage 14C Browser integration review PASS:", ", ".join(checks))
