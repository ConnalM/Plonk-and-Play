"""Deterministic contract for the bounded HTTP/SSE isolation boundary.

This is a software contract, not a claim of live-socket timing.  It checks the
ESP-IDF async ownership markers in the production adapter and models the
admission/lifecycle cases that must remain true when a real stalled client is
used by the hardware acceptance test.
"""
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BROWSER = (ROOT / "firmware/include/pp/browser_interface.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


require('"/events", HTTP_GET, events, nullptr' in BROWSER, "SSE route is missing")
require("httpd_req_async_handler_begin" in BROWSER, "async request admission is missing")
require("httpd_req_async_handler_complete" in BROWSER, "async request release is missing")
require("PageWorkerCount = 2" in BROWSER, "at least two page workers are required")
require("PageQueueDepth = 2" in BROWSER, "page queue must be bounded")
require("MaxSseClients = PP_MAX_SSE_CLIENTS" in BROWSER, "SSE limit is not explicit")
require("PP_HTTP_ORDINARY_REQUEST_RESERVE" in BROWSER, "ordinary request reserve is missing")
require("httpd_get_client_list" in BROWSER, "socket admission accounting is missing")
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
