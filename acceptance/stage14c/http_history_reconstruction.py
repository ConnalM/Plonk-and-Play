"""Stage 14C real HTTP History reconstruction regression.

This drives the production Browser routes through the Wokwi/private gateway.
The companion scenario reboots the controller after the first History request;
the second set of requests therefore exercises persisted reconstruction.
"""
import argparse
import json
import sys
import time
from http.cookiejar import CookieJar
from urllib.error import HTTPError, URLError
from urllib.request import Request, build_opener, ProxyHandler, HTTPCookieProcessor

parser = argparse.ArgumentParser()
parser.add_argument("--base-url", default="http://127.0.0.1:9080")
parser.add_argument("--timeout", type=float, default=5.0)
args = parser.parse_args()
base = args.base_url.rstrip("/")

def client():
    jar = CookieJar()
    return build_opener(ProxyHandler({}), HTTPCookieProcessor(jar))

browser = client()

def call(path, method="GET", body=None, timeout=None):
    payload = None if body is None else json.dumps(body).encode()
    request = Request(base + path, data=payload, method=method,
                      headers={"Content-Type": "application/json", "Connection": "close"})
    with browser.open(request, timeout=timeout or args.timeout) as response:
        raw = response.read().decode()
        return response.status, (json.loads(raw) if raw else None)

def wait_for(path, predicate, seconds=15):
    deadline = time.time() + seconds
    last = None
    while time.time() < deadline:
        try:
            status, value = call(path)
            last = (status, value)
            if status == 200 and predicate(value):
                return value
        except (OSError, URLError, TimeoutError):
            pass
        time.sleep(.1)
    raise AssertionError(f"timeout waiting for {path}: {last}")

def wait_for_reboot_and_health(seconds=30):
    deadline = time.time() + seconds
    saw_unavailable = False
    while time.time() < deadline:
        try:
            status, value = call("/health", timeout=1.0)
            if saw_unavailable and status == 200 and value and value.get("ok") is True:
                return
        except (OSError, URLError, TimeoutError):
            saw_unavailable = True
        time.sleep(.1)
    raise AssertionError("reboot did not produce an observable HTTP outage and recovery")

def fixture(action):
    status, value = call("/fixture", "POST", {"action": action})
    assert status == 200 and value and value.get("accepted") is True, (status, value)

def operation(path, correlation):
    status, _ = call(path, "POST", {"correlationId": correlation})
    assert status == 202, (path, status)
    deadline = time.time() + 8
    while time.time() < deadline:
        status, value = call(f"/request-result?correlationId={correlation}")
        if status == 200:
            assert value["result"] == "ACCEPTED", value
            return value
        assert status == 204, (status, value)
        time.sleep(.05)
    raise AssertionError(f"request result timeout: {path}")

def history_result_set(label):
    status, history = call("/history")
    assert status == 200 and isinstance(history, dict), (label, status, history)
    entries = history.get("entries")
    assert isinstance(entries, list) and entries and entries[-1].get("sealed") is True, (label, history)
    status, results = call("/results")
    assert status == 200 and results.get("sealed") is True and results.get("entries"), (label, status, results)
    status, details = call("/details")
    assert status == 200 and details.get("sealed") is True and any(e.get("records") for e in details.get("entries", [])), (label, status, details)
    return history, results, details

try:
    status, health = call("/health")
    assert status == 200 and health.get("ok") is True, (status, health)
    status, context = call("/context")
    assert status == 200, context
    if context.get("role") != "Race Director":
        status, _ = call("/bootstrap", "POST")
        assert status == 200
    fixture("reset")
    fixture("target3")
    operation("/request/start", 140301)
    wait_for("/state", lambda s: s.get("lifecycle") == "RACING")
    # The first crossing establishes the timing origin; the next three complete
    # a genuine three-lap race through the normal simulated Input boundary.
    for i in range(4):
        fixture("lane1")
        time.sleep(.25)
    finished = wait_for("/state", lambda s: s.get("lifecycle") == "FINISHED")
    assert finished.get("resultSealed") is True and finished.get("entries", [{}])[0].get("laps") == 3, finished
    before = history_result_set("before-reboot")
    print("Stage 14C live HTTP before-reboot PASS: /history /results /details reconstructed sealed result")
    # The companion Wokwi scenario performs the serial soft reset after this
    # point.  Polling /health naturally spans the unavailable interval.
    wait_for_reboot_and_health()
    after = history_result_set("after-reboot")
    assert after[0] == before[0], (before[0], after[0])
    assert after[1] == before[1] and after[2] == before[2], "reboot reconstruction changed Results/Details"
    print("Stage 14C live HTTP after-reboot PASS: persisted /history /results /details reconstructed identically")
except (AssertionError, OSError, URLError, HTTPError) as error:
    print("Stage 14C live HTTP History reconstruction FAIL:", error, file=sys.stderr)
    sys.exit(1)
