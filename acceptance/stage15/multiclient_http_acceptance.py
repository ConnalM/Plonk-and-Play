"""Production HTTP/SSE multi-client controller-lease acceptance.

This uses the same cookie-store and HTTP boundary as the Stage 11–13
acceptance probes.  It deliberately does not model Race Control locally.
Run only against an explicitly authorised, idle physical acceptance controller
with the simulation-arm callback enabled, e.g.:

    python multiclient_http_acceptance.py http://192.168.4.1 --acceptance

A retained disconnected owner is normal. Never erase NVS/records to obtain a
fresh lease. These synthetic passages prove HTTP/SSE, not real detectors or
official record writes. Test setup and ownership may be changed.
"""

from __future__ import annotations

import json
import queue
import socket
import sys
import threading
import time
from http.cookiejar import CookieJar
from urllib.error import HTTPError, URLError
from urllib.request import (Request, HTTPCookieProcessor, ProxyHandler,
                            build_opener)


BASE = sys.argv[1].rstrip("/") if len(sys.argv) > 1 else ""


def require_authorised_target():
    if len(sys.argv) != 3 or sys.argv[2] != "--acceptance" or not BASE.startswith("http://"):
        raise SystemExit("Supply the authorised idle acceptance-controller HTTP URL and --acceptance; no default endpoint.")


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


def operation(opener, path, correlation, timeout=5, **fields):
    status, value = call(opener, path, "POST", {"correlationId": correlation, **fields})
    assert status == 202, (path, status, value)
    deadline = time.time() + timeout
    while time.time() < deadline:
        status, value = call(opener, f"/request-result?correlationId={correlation}")
        if status == 200:
            return value
        assert status == 204, (path, status, value)
        time.sleep(0.05)
    raise AssertionError(f"request result timeout for {path}")


def fixture(opener, action, timeout=5, accepted=True, **fields):
    """Queue an acceptance fixture command and wait for main-loop execution.

    HTTPD acknowledgement means only that the immutable command was admitted.
    The correlated result distinguishes the later main-loop execution outcome;
    callers may explicitly expect an execution-time rejection after admission.
    """
    status, value = call(opener, "/fixture", "POST", {"action": action, **fields})
    assert status == 202 and value and value.get("state") == "QUEUED", (action, status, value)
    request_id = value.get("requestId")
    assert isinstance(request_id, int) and request_id > 0, value
    deadline = time.time() + timeout
    while time.time() < deadline:
        status, outcome = call(opener, f"/fixture-result?requestId={request_id}")
        if status == 503:
            time.sleep(0.05)
            continue
        assert status == 200 and outcome, (action, status, outcome)
        if not outcome.get("complete"):
            assert outcome.get("state") == "QUEUED", outcome
            time.sleep(0.05)
            continue
        assert outcome.get("accepted") is accepted, (action, outcome)
        return outcome
    raise AssertionError(f"fixture command timeout for {action}")


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
        self.remote_closed = threading.Event()
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
            self.response = self.opener.open(request, timeout=35)
            assert self.response.status == 200 and self.response.headers.get_content_type() == "text/event-stream"
            self.started.set()
            data = []
            while not self.stop_requested.is_set():
                raw = self.response.readline()
                if not raw:
                    self.remote_closed.set()
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
            if isinstance(error, ConnectionResetError) and not self.stop_requested.is_set():
                self.remote_closed.set()
            self.events.put(error)
            self.started.set()
        finally:
            if self.response is not None:
                self.response.close()
            self.closed.set()

    def discard_initial(self):
        """Discard the initial revision event, retaining later notifications."""
        event = self.wait_notification(timeout=5)
        assert isinstance(event["revision"], int), event
        return event

    def wait_notification(self, timeout=10, predicate=None):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                event = self.events.get(timeout=max(0.05, deadline - time.monotonic()))
            except queue.Empty:
                break
            if isinstance(event, Exception):
                raise AssertionError(f"{self.name} SSE failed: {event}") from event
            if isinstance(event, dict) and "revision" in event and (predicate is None or predicate(event)):
                return event
        raise AssertionError(f"{self.name} did not receive an invalidation")

    def close(self):
        self.stop_requested.set()
        # Interrupt the blocking reader before closing its buffered response.
        # CPython urllib HTTPResponse exposes the underlying connected socket.
        response = self.response
        raw = getattr(getattr(response, "fp", None), "raw", None)
        sock = getattr(raw, "_sock", None)
        if sock is not None:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
        self.thread.join(timeout=5)
        assert self.closed.is_set(), f"{self.name} SSE reader did not stop"

def assert_roles(owner, spectator):
    owner_context = context(owner)
    spectator_context = context(spectator)
    assert owner_context.get("isMaster") is True, owner_context
    assert owner_context.get("role") == "Race Director", owner_context
    assert spectator_context.get("isMaster") is False, spectator_context
    assert spectator_context.get("role") == "Spectator", spectator_context

def get_json(opener, path):
    status, value = call(opener, path)
    assert status == 200 and isinstance(value, dict), (path, status, value)
    return value


def wait_until(probe, predicate, timeout=25):
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        last = probe()
        if predicate(last):
            return last
        time.sleep(0.1)
    raise AssertionError(f"controller condition not met: {last}")


def acquire_owner(owner, spectator):
    current = context(owner)
    assert not current["ownerConnected"] and not current["takeoverPending"], (
        "Close existing controlling Browser and wait for disconnect; do not seize active control", current)
    if current["hasMaster"]:
        status, value = call(owner, "/request/take-control", "POST")
        assert status == 200 and value["result"] == "ACCEPTED", (status, value)
    else:
        status, value = call(owner, "/bootstrap", "POST")
        assert status == 200 and value["bootstrap"] is True, (status, value)
    wait_until(lambda: context(owner), lambda c: c["ownerConnected"] and c["isMaster"])
    assert_roles(owner, spectator)


def set_proposal(owner, correlation, mode, target, duration, finish):
    proposal = get_json(owner, "/proposal")
    result = operation(owner, "/request/setup", correlation,
                       mode=mode, lapTarget=target, durationMinutes=duration,
                       finishPolicy=finish, proposalRevision=proposal["proposalRevision"])
    assert result["result"] == "ACCEPTED", result
    updated = get_json(owner, "/proposal")
    assert updated["mode"] == mode and updated["lapTarget"] == target and updated["durationMinutes"] == duration
    assert updated["finishBehaviour"] == ("IMMEDIATE", "COMPLETE_CURRENT_LAP", "COMPLETE_FULL_RACE_DISTANCE")[finish]
    return updated


def arm_simulation(owner):
    assert state(owner)["lifecycle"] == "READY", "Only arm a READY acceptance controller"
    fixture(owner, "arm")
    p = get_json(owner, "/proposal")
    assert p["simulatedSession"] is True and p["startable"] is True, p
    return p


def proposal_identity(proposal):
    """Stable proposal fields that a volatile fixture reset must preserve."""
    return tuple(proposal[key] for key in (
        "mode", "lapTarget", "durationMinutes", "finishBehaviour",
        "activeLanes", "simulatedSession", "entries"))


def start_simulated(owner, correlation):
    # Explicitly validate frozen source after START before any synthetic input.
    assert get_json(owner, "/proposal")["simulatedSession"] is True
    start = operation(owner, "/request/start", correlation)
    assert start["result"] == "ACCEPTED", start
    racing = wait_state(owner, "RACING", timeout=20)
    assert racing["simulatedSession"] is True, racing
    return racing


def record_snapshot(owner):
    history = get_json(owner, "/history")
    r = get_json(owner, "/records")
    # entries[] is sized by current active entry count, so compare the stable
    # official record summaries for the exact two-lane test configuration.
    return history, tuple(r[key] for key in ("era", "lane1Pb", "lane2Pb", "trackRecord"))


def passage(owner, lane):
    before = state(owner)
    assert before["lifecycle"] == "RACING" and before["simulatedSession"] is True, before
    revision_before = get_json(owner, "/noticeboard")["revision"]
    entry = next(e for e in before["entries"] if e["lane"] == lane)
    fixture(owner, f"lane{lane}")
    after = wait_until(lambda: state(owner), lambda v: any(
        e["raceEntryId"] == entry["raceEntryId"] and e["laps"] == entry["laps"] + 1
        for e in v["entries"]), timeout=5)
    # The fixture holds the simulated source asserted for 150 ms; permit release
    # before another passage even if the lap count is visible immediately.
    time.sleep(0.25)
    fact = get_json(owner, "/fact")
    assert fact["type"] == "LAP_COMPLETED" and fact["raceEntryId"] == entry["raceEntryId"] and fact["lapNumber"] == entry["laps"] + 1, fact
    revision = wait_until(lambda: get_json(owner, "/noticeboard"),
                          lambda v: v["revision"] > revision_before, timeout=5)["revision"]
    return after, revision


def session_identity(s):
    # /state has no sessionId in this firmware; fresh Race Entry IDs and
    # committed GO are the authoritative per-race reconstruction identity.
    return s["scheduledGo"], tuple((e["lane"], e["raceEntryId"]) for e in s["entries"])


def paired_lap(a_stream, b_stream, revision):
    # Ordinary SSE invalidations expose a revision; lap/event IDs on the
    # wire are Stage14C-specific instrumentation, not a general contract.
    # passage() separately verifies the exact factual lap and current state.
    matches = lambda e: e["revision"] == revision
    ea = a_stream.wait_notification(predicate=matches)
    eb = b_stream.wait_notification(predicate=matches)
    assert ea["revision"] == eb["revision"] == revision, (ea, eb)


def restore_proposal(owner, original, correlation):
    # Only clear the test's own volatile race. Never erase NVS/history/records.
    fixture(owner, "reset")
    wait_state(owner, "READY")
    finish = ("IMMEDIATE", "COMPLETE_CURRENT_LAP", "COMPLETE_FULL_RACE_DISTANCE").index(original["finishBehaviour"])
    restored = set_proposal(owner, correlation, original["mode"], original["lapTarget"],
                            original["durationMinutes"], finish)
    assert proposal_identity(restored) == proposal_identity(original), (
        "Fixture reset did not restore the original proposal and arm state; stop for review", restored, original)


def main():
    require_authorised_target()
    a, jar_a = client()
    b, jar_b = client()

    original = get_json(a, "/proposal")
    assert state(a)["lifecycle"] == "READY" and original["activeLanes"] == 2
    assert isinstance(original["simulatedSession"], bool), original
    assert not context(a)["ownerConnected"], "Close the existing controlling Browser first"
    context(b)
    assert len(list(jar_a)) == 1 and len(list(jar_b)) == 1
    assert list(jar_a)[0].value != list(jar_b)[0].value
    sse_a = EventStream(a, "A").start()
    sse_b = EventStream(b, "B").start()
    sse_a.discard_initial()
    sse_b.discard_initial()
    acquire_owner(a, b)
    records = record_snapshot(a)
    arm_simulation(a)  # fail before altering remembered race settings if unavailable
    set_proposal(a, 3000, "LAP_RACE", 10, max(1, original["durationMinutes"]), 2)
    racing = start_simulated(a, 3001)
    session_id = session_identity(racing)

    # Scenario A: spectator action is rejected, while both streams receive a
    # real state invalidation from the same fixture passage.
    denied = operation(b, "/request/pause", 3002)
    assert denied.get("result") == "REJECTED", denied
    observed, revision = passage(a, 1)
    paired_lap(sse_a, sse_b, revision)
    after = state(b)
    assert session_identity(after) == session_id and after["lifecycle"] == "RACING", after
    assert after["entries"] == observed["entries"], (after, observed)

    # Scenario B: one pending takeover, delivered to the actual controller,
    # then KEEP leaves authority and race state unchanged.
    status, pending = call(b, "/request/take-control", "POST")
    assert status == 202 and pending.get("result") == "PENDING", pending
    assert context(a).get("takeoverPending") is True
    assert context(b).get("takeoverRequestedByThis") is True
    status, kept = call(a, "/request/take-control/keep", "POST")
    assert status == 200 and kept.get("result") == "REFUSED", kept
    assert_roles(a, b)
    assert session_identity(state(a)) == session_id

    # Scenario C: ALLOW atomically moves the lease; the former controller
    # remains a live spectator and the race continues without reconstruction.
    status, pending = call(b, "/request/take-control", "POST")
    assert status == 202 and pending.get("result") == "PENDING", pending
    status, allowed = call(a, "/request/take-control/allow", "POST")
    assert status == 200 and allowed.get("result") == "ACCEPTED", allowed
    assert_roles(b, a)
    assert context(b).get("ownerConnected") is True
    assert session_identity(state(a)) == session_id and session_identity(state(b)) == session_id
    denied_after_transfer = operation(a, "/request/pause", 3003)
    assert denied_after_transfer.get("result") == "REJECTED", denied_after_transfer
    _, revision = passage(b, 2)
    paired_lap(sse_a, sse_b, revision)
    assert state(a).get("lifecycle") == "RACING"

    # Scenario D: replacing the controlling client's SSE with the same cookie
    # identity preserves the lease and retires only the old generation.
    old_b = sse_b
    sse_b = EventStream(b, "B-reconnect").start()
    sse_b.discard_initial()
    assert old_b.remote_closed.wait(5), "server did not close the replaced SSE generation"
    assert old_b.closed.wait(5), "old controller reader remained open"
    assert_roles(b, a)
    assert context(b).get("ownerConnected") is True

    # Scenario E: closing the genuine controlling connection makes deliberate
    # reacquisition eligible.  The former owner has no automatic priority on
    # reconnect and remains a spectator after A acquires the lease.
    sse_b.close()
    wait_until(lambda: context(a), lambda c: c["ownerConnected"] is False, timeout=25)
    status, reacquired = call(a, "/request/take-control", "POST")
    assert status == 200 and reacquired.get("result") == "ACCEPTED", reacquired
    assert context(a).get("isMaster") is True
    assert context(b).get("isMaster") is False
    assert context(a).get("ownerConnected") is True
    returned_state = state(a)
    assert session_identity(returned_state) == session_id and returned_state.get("lifecycle") == "RACING", returned_state
    returned_b = EventStream(b, "B-return").start()
    returned_b.discard_initial()
    assert_roles(a, b)
    assert context(a).get("ownerConnected") is True
    assert record_snapshot(a) == records, "synthetic sessions altered official records"
    restore_proposal(a, original, 3004)
    assert record_snapshot(a) == records
    returned_b.close()
    sse_a.close()
    print("Stage 15 real-controller HTTP/SSE multi-client acceptance PASS (simulated input only)")
    print("scenarios=A controller/spectator, B KEEP, C ALLOW, D reconnect, E disconnect/reacquire")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, HTTPError, URLError, OSError, TimeoutError) as error:
        print(f"Stage 15 real-controller HTTP/SSE acceptance FAIL; inspect live controller, do not automatically reset or erase: {error}")
        raise
