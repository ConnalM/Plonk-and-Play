"""Stage 14C default-capacity production HTTP regression.

This is intentionally a real-server test.  Point it at the Wokwi/private
gateway while a freshly booted stage14cdemo image is running; it is not a
replacement for the JavaScript polling tests.
"""
import argparse
import json
import threading
import time
from http.cookiejar import CookieJar
from urllib.error import HTTPError, URLError
from urllib.request import Request, ProxyHandler, HTTPCookieProcessor, build_opener

parser = argparse.ArgumentParser()
parser.add_argument("--base-url", default="http://127.0.0.1:9080")
parser.add_argument("--timeout", type=float, default=5.0)
args = parser.parse_args()
base = args.base_url.rstrip("/")


def client():
    return build_opener(ProxyHandler({}), HTTPCookieProcessor(CookieJar()))


def call(opener, path, method="GET", body=None, timeout=None):
    payload = None if body is None else json.dumps(body).encode()
    request = Request(base + path, data=payload, method=method,
                      headers={"Content-Type": "application/json", "Connection": "close"})
    with opener.open(request, timeout=timeout or args.timeout) as response:
        raw = response.read().decode()
        return response.status, (json.loads(raw) if raw else None)


def page(opener, path):
    request = Request(base + path, headers={"Connection": "close"})
    with opener.open(request, timeout=args.timeout) as response:
        return response.status, response.read().decode()


def wait_state(opener, lifecycle, seconds=10):
    deadline = time.time() + seconds
    last = None
    while time.time() < deadline:
        status, last = call(opener, "/state")
        if status == 200 and last.get("lifecycle") == lifecycle:
            return last
        time.sleep(.1)
    raise AssertionError("expected %s, last=%r" % (lifecycle, last))


def operation(opener, path, correlation):
    status, value = call(opener, path, "POST", {"correlationId": correlation})
    assert status == 202, (path, status, value)
    deadline = time.time() + 8
    while time.time() < deadline:
        status, value = call(opener, "/request-result?correlationId=%d" % correlation)
        if status == 200:
            assert value.get("result") == "ACCEPTED", value
            return value
        assert status == 204, (status, value)
        time.sleep(.05)
    raise AssertionError("request-result timeout for %s" % path)


def fixture(opener, action):
    status, value = call(opener, "/fixture", "POST", {"action": action})
    assert status == 200 and value and value.get("accepted") is True, (action, status, value)


def complete_race(opener, correlation):
    fixture(opener, "reset")
    operation(opener, "/request/start", correlation)
    wait_state(opener, "RACING")
    # Default demo setup is the normal two-lap Lap Race.  The first crossing
    # establishes the timing origin; the following two complete the winner.
    for action in ("lane1", "lane2", "lane1"):
        fixture(opener, action)
        time.sleep(.2)
    state = wait_state(opener, "FINISHED")
    assert state.get("resultSealed") is True, state


def verify_result_routes(opener, label):
    status, results = call(opener, "/results")
    assert status == 200 and results.get("sealed") is True and results.get("entries"), (label, results)
    status, details = call(opener, "/details")
    assert status == 200 and details.get("sealed") is True, (label, details)
    assert any(entry.get("records") for entry in details.get("entries", [])), (label, details)
    status, health = call(opener, "/health")
    assert status == 200 and health.get("ok") is True, (label, health)


def dev_workload(stop, errors):
    opener = client()
    try:
        while not stop.is_set():
            for path in ("/fact", "/noticeboard"):
                call(opener, path, timeout=2.0)
            call(opener, "/health", timeout=2.0)
            stop.wait(.5)
    except Exception as error:  # propagate after the production sequence
        errors.append(error)


def run():
    opener = client()
    status, normal = page(opener, "/normal")
    assert status == 200 and "DEVELOPER" in normal, "normal Browser page not served"
    status, health = call(opener, "/health")
    assert status == 200 and health.get("ok") is True, health
    status, context = call(opener, "/context")
    assert status == 200, context
    if context.get("role") != "Race Director":
        status, value = call(opener, "/bootstrap", "POST")
        assert status == 200 and value.get("bootstrap") is True, value

    # DEV closed: production page and race/result routes only.
    complete_race(opener, 140801)
    verify_result_routes(opener, "DEV closed")
    operation(opener, "/request/race-again", 140802)
    wait_state(opener, "READY")
    verify_result_routes(opener, "DEV closed READY reconstruction")

    # DEV open: reproduce its bounded diagnostic workload while the normal
    # page/state/result traffic exercises the same real ESP-IDF server.
    stop = threading.Event()
    errors = []
    worker = threading.Thread(target=dev_workload, args=(stop, errors), daemon=True)
    worker.start()
    try:
        complete_race(opener, 140803)
        verify_result_routes(opener, "DEV open")
        operation(opener, "/request/race-again", 140804)
        wait_state(opener, "READY")
        verify_result_routes(opener, "DEV open READY reconstruction")
    finally:
        stop.set()
        worker.join(2)
    assert not errors, errors
    print("Stage 14C default-capacity real HTTP regression PASS")


try:
    run()
except (AssertionError, HTTPError, URLError, OSError, TimeoutError) as error:
    print("Stage 14C default-capacity real HTTP regression FAIL: %s" % error)
    raise SystemExit(1)
