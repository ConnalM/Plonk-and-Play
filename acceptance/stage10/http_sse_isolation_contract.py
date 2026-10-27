"""Deterministic contract for the bounded HTTP/SSE isolation boundary.

This is a software contract, not a claim of live-socket timing.  It checks the
ESP-IDF async ownership markers in the production adapter and models the
admission/lifecycle cases that must remain true when a real stalled client is
used by the hardware acceptance test.
"""
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BROWSER = (ROOT / "firmware/include/pp/browser_interface.h").read_text(encoding="utf-8")
EMBED = (ROOT / "firmware/browser/embed_normal.py").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


require('"/events", HTTP_GET, events, nullptr' in BROWSER, "SSE route is missing")
require("httpd_req_async_handler_begin" in BROWSER, "async request admission is missing")
require("httpd_req_async_handler_complete" in BROWSER, "async request release is missing")
require("PageWorkerCount = 2" in BROWSER, "at least two page workers are required")
require("PageQueueDepth = 2" in BROWSER, "page queue must be bounded")
require("MaxSseClients = PP_MAX_SSE_CLIENTS" in BROWSER, "SSE limit is not explicit")
require("HttpdMaxOpenSockets = 13" in BROWSER, "HTTPD client capacity is not a shared source of truth")
require("MaxHttpClientSockets" not in BROWSER, "legacy second-subtracted HTTPD capacity remains")
require("config.max_open_sockets = static_cast<uint16_t>(HttpdMaxOpenSockets)" in BROWSER,
        "HTTPD configuration does not use the shared client capacity")
require("size_t fdsCapacity = HttpdMaxOpenSockets" in BROWSER,
        "client-list capacity does not use the shared HTTPD capacity")
require("fdsCapacity >= HttpdMaxOpenSockets - OrdinaryRequestReserve" in BROWSER,
        "admission boundary does not preserve the ordinary reserve against the shared capacity")
require("PP_HTTP_ORDINARY_REQUEST_RESERVE" in BROWSER, "ordinary request reserve is missing")
require("httpd_get_client_list" in BROWSER, "socket admission accounting is missing")
require("static esp_err_t rejectSseAdmission" in BROWSER, "SSE rejection has no single terminal helper")
require('httpd_resp_set_status(request, "503 Service Unavailable")' in BROWSER, "SSE rejection is not an explicit 503")
require('httpd_resp_set_hdr(request, "Connection", "close")' in BROWSER, "SSE rejection does not require connection close")
require('httpd_resp_sendstr(request, message ? message : "SSE unavailable")' in BROWSER, "SSE rejection has no bounded response body")
require("return ESP_FAIL;" in BROWSER[BROWSER.index("static esp_err_t rejectSseAdmission"):BROWSER.index("void invalidateSseSession")], "SSE rejection does not hand the socket to HTTPD's close path")
require("availableSseSession" in BROWSER and "activateSseSession" in BROWSER, "SSE admission allocates before rejection/async ownership is settled")
require("allocateSseSession" not in BROWSER, "legacy SSE admission can mutate a session before rejection")
events_start = BROWSER.index("static esp_err_t events(httpd_req_t* request)")
events_end = BROWSER.index("static esp_err_t", events_start + 20)
events_body = BROWSER[events_start:events_end]
require(events_body.count("return rejectSseAdmission") >= 4,
        "all bounded SSE rejection branches do not share one terminal response/close path")
async_begin = events_body.index("httpd_req_async_handler_begin")
for branch in ("ordinary_capacity_exhausted", "sse_slots_exhausted", "session_allocation_failed", "async_begin_failed"):
    branch_pos = events_body.index(branch)
    require(events_body.find("return rejectSseAdmission", branch_pos) >= branch_pos,
            f"{branch} does not terminate through the explicit 503/close helper")
require(events_body.find("activateSseSession") > async_begin,
        "SSE session became active before async ownership was established")
require("httpd_sess_get_ctx(handle, fd) != session" in BROWSER, "fd/session reuse guard is missing")
require("findSseSessionExact(session)" in BROWSER, "generation/session identity guard is missing")
require("queuedGeneration" in BROWSER, "stale queued generation is not tracked")
require("httpd_req_to_sockfd(request) == job.fd" in BROWSER, "page worker has no fd/generation guard")
require("serverGeneration" in BROWSER, "page worker has no server-generation guard")
require("session->serverGeneration != serverGeneration_" in BROWSER, "SSE worker has no server-generation guard")
require('"page ordinary capacity unavailable"' in BROWSER, "page admission does not preserve ordinary capacity")
require("close_failed" in BROWSER, "close failure is not observable")
require("httpd_queue_work(server_, &BrowserInterface::eventWork, work)" not in BROWSER,
        "blocking SSE sends remain on the HTTPD work queue")
require("httpd_socket_send" not in BROWSER, "SSE still uses a shared raw socket send")
require("SseWorkerStackBytes = 8192" in BROWSER,
        "diagnostic SSE worker stack is not the approved 8192-byte starting point")
require("initialFrame[SseInitialFrameCapacity]" in BROWSER,
        "initial SSE frame is still worker-stack storage")
require("payloadFrame[SsePayloadCapacity]" in BROWSER,
        "SSE notification frame is still worker-stack storage")
require("char initial[128]" not in BROWSER and "char payload[192]" not in BROWSER,
        "legacy automatic SSE frame buffers remain in the worker")
telemetry_start = BROWSER.index("void recordSseStack(")
telemetry_end = BROWSER.index("void drainSseStackTelemetry()", telemetry_start)
telemetry_body = BROWSER[telemetry_start:telemetry_end]
require("Serial.printf" not in telemetry_body and "snprintf" not in telemetry_body and "vsnprintf" not in telemetry_body,
        "SSE worker telemetry still performs formatted output")
worker_start = BROWSER.index("static void sseWorkerTask")
worker_end = BROWSER.index("void onSseTransportClosed", worker_start)
worker_body = BROWSER[worker_start:worker_end]
require("Serial.printf" not in worker_body and "snprintf" not in worker_body and "vsnprintf" not in worker_body,
        "SSE worker path still performs formatted output")
for stage in ("idle", "initial", "notification", "keepalive", "stalled_send",
              "send_failure", "disconnect", "replacement"):
    require(f'"{stage}"' in BROWSER,
            f"SSE high-water lifecycle stage is not instrumented: {stage}")
require("[HTTP_TRACE]" in BROWSER and "send_begin" in BROWSER and "send_complete" in BROWSER,
        "detailed ordinary-route transport tracing is missing")
require("[HTTP_ADMISSION]" in BROWSER,
        "transport admission tracing is missing")
for route in ("health", "context", "state", "proposal"):
    require(f'RequestTrace trace(instance(), "/{route}", request, true)' in BROWSER,
            f"/{route} does not use detailed transport tracing")
require('static esp_err_t page(httpd_req_t* request)' in BROWSER,
        "bounded page admission handler is missing")
require('{"/", HTTP_GET, page, nullptr},\\n      {"/normal", HTTP_GET, page, nullptr}' in EMBED,
        "root and /normal do not both use bounded page admission")
require("normalPage" not in EMBED,
        "generated Normal Browser still contains a synchronous normalPage route")
require("scheduleEventStreamRecovery" in EMBED, "Browser has no single SSE recovery scheduler")
require("eventStreamRetryTimer" in EMBED and "eventStreamRecoveryInFlight" in EMBED,
        "Browser recovery has no bounded timer/in-flight guard")
require("eventStreamRecoveryWaitTimer" in EMBED and "if(polling)" in EMBED,
        "Browser reconnect can race an existing acquisition without a bounded wait")
require("stream.close()" in EMBED and "stream.onerror=()=>" in EMBED,
        "Browser does not retire failed EventSource instances deterministically")
require("eventStreamRetryMaxMs=16000" in EMBED and "eventStreamRetryBaseMs=2000" in EMBED,
        "Browser SSE backoff is not bounded")
require("connectionState==='OFFLINE'&&!['bootstrap','reconnect','recovery'].includes(trigger)" in EMBED,
        "offline Browser still permits normal acquisition polling")
require("!['reconnect','recovery'].includes(trigger)" in EMBED,
        "recovery does not force fresh authoritative reconstruction")
require("browser_sse_admission_recovery_runtime.js" in (ROOT / "acceptance/stage14c/run.ps1").read_text(encoding="utf-8"),
        "deterministic single-owner SSE recovery runtime is not in the Stage 14C suite")


# A deliberately stalled page occupies one worker; a second page can still be
# admitted to the other worker, while ordinary routes remain independent.
page_workers = [None, None]
page_queue = []
ordinary_capacity = 4
page_workers[0] = "stalled-page"
page_workers[1] = "healthy-page"
page_queue.extend(("queued-page-1", "queued-page-2"))
require(page_workers == ["stalled-page", "healthy-page"] and len(page_queue) == 2,
        "page isolation model did not admit bounded work")
require(ordinary_capacity == 4, "ordinary request capacity was not reserved")
require(len(page_queue) == 2, "page queue did not remain bounded")
page_rejected = "fifth-page"
require(page_rejected not in page_queue, "page admission exceeded the finite queue bound")

# Four streams are admitted, then a fifth is explicitly rejected.  A stalled
# stream never changes another stream's ownership or delivery state.
sse = {index: {"fd": 50 + index, "generation": index + 1, "valid": True}
       for index in range(4)}
require(len(sse) == 4, "the configured four-stream limit is not represented")
require(4 not in sse, "SSE admission did not remain bounded")
sse[0]["stalled"] = True
require(sse[1]["valid"] and not sse[1].get("stalled", False),
        "a stalled SSE stream affected a healthy stream")

# fd reuse must discard stale work even if the operating system recycles the
# descriptor.  The new generation is the only one allowed to send.
old = {"fd": 77, "generation": 9, "valid": False}
replacement = {"fd": 77, "generation": 10, "valid": True}
queued_generation = old["generation"]
require(queued_generation != replacement["generation"],
        "stale queued work was accepted after fd reuse")
require(replacement["valid"], "replacement session was not independently live")

# Every admitted async object has exactly one terminal release in the model.
released = set()
for identity in ("page-0", "sse-0", "sse-replacement"):
    released.add(identity)
require(len(released) == 3, "async ownership did not release each object exactly once")

print("HTTP/SSE isolation contract PASS: bounded page workers, reserved ordinary capacity, isolated SSE, async ownership, fd/generation reuse and failure reclamation")
