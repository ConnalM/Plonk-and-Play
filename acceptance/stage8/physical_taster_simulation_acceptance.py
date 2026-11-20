"""Physical-Taster acceptance through the opt-in shared simulation fixture.

Run only against the owner-authorised simulation acceptance image. The PC
substitutes reproducible local Taster actions and detector crossings; P&P
still projects authoritative state to the physical Taster and connected
Browser. It never erases records. The observer must witness labelled display
states: HTTP observations prove projection and authority, not pixels.
"""
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "stage15"))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from physical_acceptance_baseline import require_immutable_baseline
from multiclient_http_acceptance import (
    EventStream, acquire_owner, call, client, context, fixture,
    get_json, operation, passage, proposal_identity, record_snapshot,
    require_authorised_target, restore_proposal, wait_state, wait_until,
)

FINISHES = ("IMMEDIATE", "COMPLETE_CURRENT_LAP", "COMPLETE_FULL_RACE_DISTANCE")

def no_smug_controller(opener):
    value = context(opener)
    assert not value["ownerConnected"], (
        "Close the connected SMUG before exercising local Taster authority.", value)


def taster_setup(opener, mode, laps, minutes, finish):
    no_smug_controller(opener)
    fixture(opener, "taster-setup", mode=mode, lapTarget=laps,
            durationMinutes=minutes, finishPolicy=finish)
    return wait_until(lambda: get_json(opener, "/proposal"),
                      lambda p: p["mode"] == mode and p["lapTarget"] == laps and
                      p["durationMinutes"] == minutes and
                      p["finishBehaviour"] == FINISHES[finish])


def taster_arm_and_start(opener):
    no_smug_controller(opener)
    # The image may intentionally start pre-armed. Still call the arm callback
    # and require its response before every synthetic session.
    fixture(opener, "taster-arm")
    assert get_json(opener, "/proposal")["simulatedSession"] is True
    fixture(opener, "taster-start")
    running = wait_state(opener, "RACING", timeout=20)
    assert running["simulatedSession"] is True and running["historySequence"] == 0, running
    return running


def taster_pass(opener, lane, dwell=0.35):
    no_smug_controller(opener)
    before = get_json(opener, "/state")
    entry = next(e for e in before["entries"] if e["lane"] == lane)
    fixture(opener, f"taster-lane{lane}")
    observed = wait_until(lambda: get_json(opener, "/state"),
                          lambda state: any(e["raceEntryId"] == entry["raceEntryId"] and
                                            e["laps"] == entry["laps"] + 1
                                            for e in state["entries"]), timeout=5)
    # Firmware asserts a passage for 150 ms. Do not rearm a lane until both
    # assertion and the Input Module clear/debounce interval have elapsed.
    time.sleep(dwell)
    return observed


def restore_taster_proposal(opener, original, records):
    no_smug_controller(opener)
    fixture(opener, "taster-reset")
    wait_state(opener, "READY")
    restored = taster_setup(opener, original["mode"], original["lapTarget"],
                             original["durationMinutes"],
                             FINISHES.index(original["finishBehaviour"]))
    assert proposal_identity(restored) == proposal_identity(original), (restored, original)
    assert record_snapshot(opener) == records, "test altered official records"


def lap_pause_finish_witness(opener, original, records):
    print("WITNESS A: two-lap display; lane 1 completes before PAUSE; hold PAUSED display for 3 seconds.")
    taster_setup(opener, "LAP_RACE", 2, max(1, original["durationMinutes"]), 2)
    taster_arm_and_start(opener)
    taster_pass(opener, 1, dwell=0.75)
    after_lane_one = taster_pass(opener, 1, dwell=0.75)
    assert next(e for e in after_lane_one["entries"] if e["lane"] == 1)["laps"] == 2
    fixture(opener, "taster-pause")
    paused = wait_state(opener, "PAUSED", timeout=8)
    paused_entries = paused["entries"]
    time.sleep(3.0)
    assert get_json(opener, "/state")["entries"] == paused_entries, "input changed while visibly paused"
    fixture(opener, "taster-resume")
    wait_state(opener, "RACING", timeout=12)
    taster_pass(opener, 2, dwell=0.75)
    taster_pass(opener, 2, dwell=0.75)
    finished = wait_state(opener, "FINISHED", timeout=8)
    assert finished["resultSealed"] and all(e["laps"] == 2 for e in finished["entries"]), finished
    assert get_json(opener, "/results")["sealed"]
    assert record_snapshot(opener) == records
    restore_taster_proposal(opener, original, records)


def endurance_witness(opener, original, records, finish, label):
    print(f"WITNESS {label}: one-minute Endurance {FINISHES[finish]}; observe countdown and expiry presentation.")
    taster_setup(opener, "ENDURANCE", 2, 1, finish)
    taster_arm_and_start(opener)
    taster_pass(opener, 1, dwell=0.75)
    taster_pass(opener, 2, dwell=0.75)
    if finish == 0:
        finished = wait_state(opener, "FINISHED", timeout=80)
    else:
        expiry = wait_until(lambda: get_json(opener, "/state"),
                            lambda state: state["durationExpired"] is True, timeout=80)
        assert expiry["lifecycle"] == "RACING", expiry
        print("WITNESS: time expired; now pass each lane once to finish current lap.")
        taster_pass(opener, 1, dwell=0.75)
        taster_pass(opener, 2, dwell=0.75)
        finished = wait_state(opener, "FINISHED", timeout=12)
    assert finished["resultSealed"] and finished["historySequence"] == 0, finished
    assert record_snapshot(opener) == records
    restore_taster_proposal(opener, original, records)


def standalone_handover_witness(opener, original, records):
    print("WITNESS D: standalone simulated race remains visible while a SMUG takes control.")
    taster_setup(opener, "LAP_RACE", 2, max(1, original["durationMinutes"]), 2)
    taster_arm_and_start(opener)
    taster_pass(opener, 1, dwell=0.75)
    spectator, _ = client()
    stream = EventStream(opener, "standalone-handover").start()
    stream.discard_initial()
    acquire_owner(opener, spectator)
    current = get_json(opener, "/state")
    assert current["lifecycle"] == "RACING" and current["simulatedSession"] is True and current["historySequence"] == 0, current
    # Local action is subordinate; the controller uses the normal request path
    # and cannot promote this frozen simulated session.
    # This is admission rejection: a connected SMUG suppresses the local
    # Taster fixture before it can enter the main-loop command queue.
    assert call(opener, "/fixture", "POST", {"action": "taster-pause"})[0] == 400
    assert operation(opener, "/request/pause", 8101)["result"] == "ACCEPTED"
    wait_state(opener, "PAUSED", timeout=8)
    assert operation(opener, "/request/resume", 8102)["result"] == "ACCEPTED"
    wait_state(opener, "RACING", timeout=12)
    passage(opener, 1)
    passage(opener, 2)
    passage(opener, 2)
    finished = wait_state(opener, "FINISHED", timeout=8)
    assert finished["resultSealed"] and finished["historySequence"] == 0, finished
    assert record_snapshot(opener) == records
    restore_proposal(opener, original, 8103)
    assert record_snapshot(opener) == records
    stream.close()


def main():
    require_authorised_target()
    browser, _ = client()
    assert call(browser, "/health")[0] == 200
    original = require_immutable_baseline(get_json(browser, "/proposal"))
    assert get_json(browser, "/state")["lifecycle"] == "READY"
    assert original["simulatedSession"] is True, (
        "This physical acceptance image must be recognisably pre-armed; do not use a normal customer image.", original)
    no_smug_controller(browser)
    records = record_snapshot(browser)

    lap_pause_finish_witness(browser, original, records)
    endurance_witness(browser, original, records, 0, "B")
    endurance_witness(browser, original, records, 1, "C")
    standalone_handover_witness(browser, original, records)
    assert proposal_identity(get_json(browser, "/proposal")) == proposal_identity(original)
    assert record_snapshot(browser) == records
    print("Physical Taster simulated acceptance PASS (all display witness points observed)")


if __name__ == "__main__":
    main()
