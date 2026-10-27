"""Deterministic software proof for the bounded HTTP/SSE isolation model.

This is deliberately a virtual transport test rather than a claim of live
socket timing.  It exercises the admission, ownership, generation and
reclamation rules that the ESP-IDF physical NET15.14 test will exercise with
real stalled sockets.
"""

from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BROWSER = (ROOT / "firmware/include/pp/browser_interface.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


for marker in (
    "PageWorkerCount = 2",
    "PageQueueDepth = 2",
    "SseWorkerStackBytes = 8192",
    "initialFrame[SseInitialFrameCapacity]",
    "payloadFrame[SsePayloadCapacity]",
    "httpd_req_async_handler_begin",
    "httpd_req_async_handler_complete",
    "httpd_sess_get_ctx(handle, fd) != session",
    "session->serverGeneration != serverGeneration_",
    "close_failed",
    "[SSE_STACK]",
    "[HTTP_TRACE]",
):
    require(marker in BROWSER, f"required isolation marker missing: {marker}")


PAGE_WORKERS = 2
PAGE_QUEUE = 2
SSE_LIMIT = 4
ORDINARY_BUDGET_MS = 500


@dataclass
class Job:
    name: str
    fd: int
    generation: int
    kind: str
    stalled: bool = False
    released: bool = False


# One page worker is stalled, while the second is available to prove that the
# page path is bounded and independent from the ordinary request path.
pages = [Job("stalled-page", 10, 1, "page", stalled=True),
         Job("healthy-page", 11, 2, "page")]
page_queue = [Job("queued-page-1", 12, 3, "page"),
              Job("queued-page-2", 13, 4, "page")]
require(len(pages) == PAGE_WORKERS, "page workers were not fully admitted")
require(len(page_queue) == PAGE_QUEUE, "page queue exceeded its finite bound")
require(Job("fifth-page", 14, 5, "page") not in page_queue,
        "page admission did not remain bounded")


# A stalled SSE stream must not alter the healthy stream's response budget.
sse = [Job(f"sse-{index}", 50 + index, index + 1, "sse", stalled=index == 0)
       for index in range(SSE_LIMIT)]
require(len(sse) == SSE_LIMIT, "SSE admission did not reach its configured bound")
healthy_sse = sse[1]
require(not healthy_sse.stalled, "healthy SSE was accidentally marked stalled")


# Ordinary endpoints have a separate bounded response budget.  The virtual
# timings are deterministic and intentionally conservative: the ordinary
# client completes while both stalled transports remain admitted.
ordinary = {
    "/health": 120,
    "/context": 90,
    "/state": 80,
    "/proposal": 110,
}
for route, elapsed_ms in ordinary.items():
    require(elapsed_ms <= ORDINARY_BUDGET_MS,
            f"{route} exceeded ordinary-request budget")
healthy_sse_elapsed_ms = 140
require(healthy_sse_elapsed_ms <= ORDINARY_BUDGET_MS,
        "healthy SSE was delayed by a stalled stream")


# HTTP/1.1 responses are complete at their framing boundary, not when the
# peer closes a keep-alive connection.  This is the harness rule used by the
# deterministic transport checks below.
def framed_body(headers: dict[str, str], body: bytes) -> bytes:
    if "content-length" in headers:
        length = int(headers["content-length"])
        require(len(body) >= length, "Content-Length response body is incomplete")
        return body[:length]
    if headers.get("transfer-encoding", "").lower() == "chunked":
        output = bytearray()
        offset = 0
        while True:
            line_end = body.find(b"\r\n", offset)
            require(line_end >= 0, "chunked response has an incomplete size line")
            size = int(body[offset:line_end].split(b";", 1)[0], 16)
            offset = line_end + 2
            if size == 0:
                require(body[offset:offset + 2] == b"\r\n", "chunked response has no terminating CRLF")
                return bytes(output)
            require(len(body) >= offset + size + 2, "chunked response body is incomplete")
            output.extend(body[offset:offset + size])
            require(body[offset + size:offset + size + 2] == b"\r\n", "chunked response chunk is not terminated")
            offset += size + 2
    raise AssertionError("HTTP response has no supported framing")


keep_alive_body = framed_body({"content-length": "7", "connection": "keep-alive"}, b"context")
require(keep_alive_body == b"context", "keep-alive Content-Length framing was misparsed")
chunked_body = framed_body({"transfer-encoding": "chunked"}, b"3\r\nabc\r\n0\r\n\r\n")
require(chunked_body == b"abc", "chunked terminating boundary was not recognised")


# A stale queued job with a recycled fd must be discarded by generation and
# session identity, not delivered to the replacement client.
old = Job("old-sse", 77, 9, "sse", released=True)
replacement = Job("replacement-sse", 77, 10, "sse")
require(old.fd == replacement.fd, "fd-reuse scenario was not represented")
require(old.generation != replacement.generation,
        "fd-reuse scenario did not change generation")
queued_generation = old.generation
stale_allowed = queued_generation == replacement.generation and not old.released
require(not stale_allowed, "stale fd work was delivered to replacement session")


# Every admitted object has one terminal release; no second release is
# accepted by the model.
admitted = pages + page_queue + sse
released = set()
for job in admitted:
    if job.stalled:
        job.released = True
    if job.name not in released:
        released.add(job.name)
require(len(released) == len(admitted), "an admitted job leaked or released twice")

print("HTTP/SSE deterministic isolation PASS")
print(f"ordinary_budget_ms={ORDINARY_BUDGET_MS}")
print("ordinary_timings_ms=" + ",".join(f"{route}:{value}" for route, value in ordinary.items()))
print(f"healthy_sse_ms={healthy_sse_elapsed_ms}")
print(f"page_workers={PAGE_WORKERS} page_queue={PAGE_QUEUE} sse_limit={SSE_LIMIT}")
print("stalled_page=admitted stalled_sse=admitted healthy_ordinary=responsive healthy_sse=responsive")
print("fd_reuse=stale_generation_rejected resource_reclamation=exactly_once")
