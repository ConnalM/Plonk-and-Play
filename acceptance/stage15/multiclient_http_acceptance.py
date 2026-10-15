"""Production HTTP/SSE multi-client controller-lease acceptance.

This uses the same cookie-store and HTTP boundary as the Stage 11–13
acceptance probes.  It deliberately does not model Race Control locally.
Run it against a freshly booted acceptance controller, for example:

    python multiclient_http_acceptance.py http://127.0.0.1:9080
"""

from __future__ import annotations

import json
import queue
import sys
import threading
import time
from http.cookiejar import CookieJar
from urllib.error import HTTPError, URLError
from urllib.request import (Request, HTTPCookieProcessor, ProxyHandler,
                            build_opener)


BASE = sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:9080"


def client():
    jar = CookieJar()
    return build_opener(ProxyHandler({}), HTTPCookieProcessor(jar)), jar


def call(opener, path, method="GET", body=None, timeout=5):
    data = None if body is None else json.dumps(body).encode()
    headers = {"Content-Type": "application/json", "Connection": "close"}
    request = Request(BASE + path, data=data, method=method, headers=headers)
    try:
        with opener.open(request, timeout=timeout) as response:
            raw = response.read().decode()
            return response.status, json.loads(raw) if raw else None
    except HTTPError as error:
        raw = error.read().decode()
        try:
            value = json.loads(raw) if raw else None
        except ValueError:
            value = raw
        return error.code, value


def state(opener):
    status, value = call(opener, "/state")
    assert status == 200 and isinstance(value, dict), (status, value)
    return value


def context(opener):
    status, value = call(opener, "/context")
    assert status == 200 and isinstance(value, dict), (status, value)
    return value


def wait_state(opener, lifecycle, timeout=8):
    deadline = time.time() + timeout
    last = None
    while time.time() < deadline:
        last = state(opener)
        if last.get("lifecycle") == lifecycle:
            return last
        time.sleep(0.1)
    raise AssertionError(f"expected {lifecycle}, last={last}")


def operation(opener, path, correlation, timeout=5):
    status, value = call(opener, path, "POST", {"correlationId": correlation})
    assert status == 202, (path, status, value)
    deadline = time.time() + timeout
    while time.time() < deadline:
        status, value = call(opener, f"/request-result?correlationId={correlation}")
        if status == 200:
            return value
        assert status == 204, (path, status, value)
        time.sleep(0.05)
    raise AssertionError(f"request result timeout for {path}")


class EventStream:
    """Small reader for one real persistent /events response."""

    def __init__(self, opener, name):
        self.opener = opener
        self.name = name
        self.events = queue.Queue()
        self.stop_requested = threading.Event()
        self.started = threading.Event()
        self.closed = threading.Event()
        self.response = None
        self.thread = threading.Thread(target=self._read, daemon=True)

    def start(self):
        self.thread.start()
        assert self.started.wait(5), f"{self.name} SSE did not start"
        return self

    def _read(self):
        try:
            request = Request(
                BASE + "/events",
                headers={
                    "Accept": "text/event-stream",
                    "Cache-Control": "no-cache",
                    "Connection": "keep-alive",
                },
            )
            self.response = self.opener.open(request, timeout=10)
            self.started.set()
            data = []
            while not self.stop_requested.is_set():
                raw = self.response.readline()
                if not raw:
                    break
                line = raw.decode(errors="replace").rstrip("\r\n")
                if line.startswith("data:"):
                    data.append(line[5:].strip())
                elif line == "" and data:
                    payload = "\n".join(data)
                    try:
                        self.events.put(json.loads(payload))
                    except ValueError:
                        self.events.put(payload)
                    data = []
        except Exception as error:  # surfaced by wait/close assertions
            self.events.put(error)
            self.started.set()
        finally:
            self.closed.set()

    def discard_initial(self):
        """Discard the initial revision event, retaining later notifications."""
        try:
            self.events.get(timeout=3)
        except queue.Empty as error:
            raise AssertionError(f"{self.name} missing initial SSE event") from error

    def wait_notification(self, timeout=5):
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                event = self.events.get(timeout=max(0.05, deadline - time.time()))
            except queue.Empty:
                break
            if isinstance(event, Exception):
                raise AssertionError(f"{self.name} SSE failed: {event}") from event
            if isinstance(event, dict) and "revision" in event:
                return event
        raise AssertionError(f"{self.name} did not receive an invalidation")

    def close(self):
        self.stop_requested.set()
        if self.response is not None:
            try:
                self.response.close()
            except Exception:
                pass
        self.thread.join(timeout=2)


def fixture(opener, action):
    status, value = call(opener, "/fixture", "POST", {"action": action})
    assert status == 200, (action, status, value)
    return value


def assert_roles(owner, spectator):
    owner_context = context(owner)
    spectator_context = context(spectator)
    assert owner_context.get("isMaster") is True, owner_context
    assert owner_context.get("role") == "Race Director", owner_context
    assert spectator_context.get("isMaster") is False, spectator_context
    assert spectator_context.get("role") == "Spectator", spectator_context


def main():
    a, jar_a = client()
    b, jar_b = client()

    # A fresh acceptance controller starts with no retained Master.  The
    # cookie jars prove independent Browser identities, rather than tabs in a
    # shared profile.
    assert context(a).get("hasMaster") is False
    assert context(b).get("hasMaster") is False
    assert len(list(jar_a)) == 1 and len(list(jar_b)) == 1
    status, bootstrap = call(a, "/bootstrap", "POST")
    assert status == 200 and bootstrap.get("bootstrap") is True, bootstrap
    assert_roles(a, b)

    sse_a = EventStream(a, "A").start()
    sse_b = EventStream(b, "B").start()
    sse_a.discard_initial()
    sse_b.discard_initial()

    fixture(a, "reset")
    start = operation(a, "/request/start", 3001)
    assert start.get("result") == "ACCEPTED", start
    racing = wait_state(a, "RACING")
    session_id = racing.get("sessionId")

    # Scenario A: spectator action is rejected, while both streams receive a
    # real state invalidation from the same fixture passage.
    denied = operation(b, "/request/pause", 3002)
    assert denied.get("result") == "REJECTED", denied
    before = state(a)
    fixture(a, "lane1")
    event_a = sse_a.wait_notification()
    event_b = sse_b.wait_notification()
    after = state(b)
    assert event_a.get("revision") == event_b.get("revision"), (event_a, event_b)
    assert after.get("sessionId") == session_id and after.get("lifecycle") == "RACING", after
    assert after.get("entries", [{}])[0].get("laps", 0) > before.get("entries", [{}])[0].get("laps", 0)

    # Scenario B: one pending takeover, delivered to the actual controller,
    # then KEEP leaves authority and race state unchanged.
    status, pending = call(b, "/request/take-control", "POST")
    assert status == 202 and pending.get("result") == "PENDING", pending
    assert context(a).get("takeoverPending") is True
    assert context(b).get("takeoverRequestedByThis") is True
    status, kept = call(a, "/request/take-control/keep", "POST")
    assert status == 200 and kept.get("result") == "REFUSED", kept
    assert_roles(a, b)
    assert state(a).get("sessionId") == session_id

    # Scenario C: ALLOW atomically moves the lease; the former controller
    # remains a live spectator and the race continues without reconstruction.
    status, pending = call(b, "/request/take-control", "POST")
    assert status == 202 and pending.get("result") == "PENDING", pending
    status, allowed = call(a, "/request/take-control/allow", "POST")
    assert status == 200 and allowed.get("result") == "ACCEPTED", allowed
    assert_roles(b, a)
    assert state(a).get("sessionId") == session_id and state(b).get("sessionId") == session_id
    denied_after_transfer = operation(a, "/request/pause", 3003)
    assert denied_after_transfer.get("result") == "REJECTED", denied_after_transfer
    fixture(b, "lane2")
    event_a = sse_a.wait_notification()
    event_b = sse_b.wait_notification()
    assert event_a.get("revision") == event_b.get("revision"), (event_a, event_b)
    assert state(a).get("lifecycle") == "RACING"

    # Scenario D: replacing the controlling client's SSE with the same cookie
    # identity preserves the lease and retires only the old generation.
    old_b = sse_b
    sse_b = EventStream(b, "B-reconnect").start()
    sse_b.discard_initial()
    old_b.close()
    assert old_b.closed.wait(2), "old controller SSE generation remained open"
    assert_roles(b, a)

    # Scenario E: closing the genuine controlling connection makes deliberate
    # reacquisition eligible.  The former owner has no automatic priority on
    # reconnect and remains a spectator after A acquires the lease.
    sse_b.close()
    deadline = time.time() + 5
    while time.time() < deadline and context(a).get("ownerConnected") is True:
        time.sleep(0.1)
    assert context(a).get("ownerConnected") is False, context(a)
    status, reacquired = call(a, "/request/take-control", "POST")
    assert status == 200 and reacquired.get("result") == "ACCEPTED", reacquired
    assert context(a).get("isMaster") is True
    assert context(b).get("isMaster") is False
    returned_state = state(a)
    assert returned_state.get("sessionId") == session_id and returned_state.get("lifecycle") == "RACING", returned_state
    returned_b = EventStream(b, "B-return").start()
    returned_b.discard_initial()
    assert_roles(a, b)
    returned_b.close()
    sse_a.close()
    print("Stage 15 production HTTP/SSE multi-client acceptance PASS")
    print("scenarios=A controller/spectator, B KEEP, C ALLOW, D reconnect, E disconnect/reacquire")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, HTTPError, URLError, OSError, TimeoutError) as error:
        print(f"Stage 15 production HTTP/SSE multi-client acceptance FAIL: {error}")
        raise
