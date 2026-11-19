"""Physical-Taster acceptance through the opt-in shared simulation fixture.

This script has no embedded timings or expected presentation implementation.
It sends accepted Taster-role requests and simulated crossings to a separately
authorised acceptance image, then observes the same public Browser state and
record views used by a connected SMUG.  It never erases records or settings.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "stage15"))
from multiclient_http_acceptance import (call, client, context, get_json,
                                         record_snapshot, require_authorised_target,
                                         wait_state, wait_until)


def fixture(opener, action, **fields):
    status, value = call(opener, "/fixture", "POST", {"action": action, **fields})
    assert status == 200 and value and value.get("accepted") is True, (action, status, value)
    return value


def taster_setup(opener, mode, laps, minutes, finish):
    fixture(opener, "taster-setup", mode=mode, lapTarget=laps,
            durationMinutes=minutes, finishPolicy=finish)
    return wait_until(lambda: get_json(opener, "/proposal"),
                      lambda p: p["mode"] == mode and p["lapTarget"] == laps and
                      p["durationMinutes"] == minutes and
                      p["finishBehaviour"] == ("IMMEDIATE", "COMPLETE_CURRENT_LAP", "COMPLETE_FULL_RACE_DISTANCE")[finish])


def taster_pass(opener, lane):
    before = get_json(opener, "/state")
    entry = next(e for e in before["entries"] if e["lane"] == lane)
    fixture(opener, f"taster-lane{lane}")
    return wait_until(lambda: get_json(opener, "/state"),
                      lambda state: any(e["raceEntryId"] == entry["raceEntryId"] and
                                        e["laps"] == entry["laps"] + 1
                                        for e in state["entries"]), timeout=5)


def main():
    require_authorised_target()
    browser, _ = client()
    assert call(browser, "/health")[0] == 200
    original = get_json(browser, "/proposal")
    assert get_json(browser, "/state")["lifecycle"] == "READY"
    assert not context(browser)["ownerConnected"], (
        "Close the controlling SMUG first: this test verifies standalone Taster authority.")
    records = record_snapshot(browser)

    # This is the real Taster setup/start/pause/resume ingress.  The PC only
    # supplies the reproducible acceptance gesture; it never acquires control.
    taster_setup(browser, "LAP_RACE", 2, max(1, original["durationMinutes"]), 2)
    fixture(browser, "taster-arm")
    assert get_json(browser, "/proposal")["simulatedSession"] is True
    fixture(browser, "taster-start")
    racing = wait_state(browser, "RACING", timeout=20)
    assert racing["simulatedSession"] is True and racing["historySequence"] == 0, racing

    fixture(browser, "taster-pause")
    wait_state(browser, "PAUSED", timeout=8)
    fixture(browser, "taster-resume")
    wait_state(browser, "RACING", timeout=12)

    taster_pass(browser, 1)
    taster_pass(browser, 2)
    taster_pass(browser, 1)
    taster_pass(browser, 2)
    finished = wait_state(browser, "FINISHED", timeout=8)
    assert finished["resultSealed"] and all(e["laps"] == 2 for e in finished["entries"]), finished
    assert get_json(browser, "/results")["sealed"]
    assert record_snapshot(browser) == records, "simulated standalone test altered official records"

    # Return the test's volatile session and its remembered setup to READY.
    fixture(browser, "taster-reset")
    wait_state(browser, "READY")
    taster_setup(browser, original["mode"], original["lapTarget"],
                 original["durationMinutes"],
                 ("IMMEDIATE", "COMPLETE_CURRENT_LAP", "COMPLETE_FULL_RACE_DISTANCE").index(original["finishBehaviour"]))
    assert record_snapshot(browser) == records
    print("Physical Taster simulated acceptance PASS (Taster and Browser observed; visual screen witness remains required)")


if __name__ == "__main__":
    main()
