"""Stage 13 real-controller HTTP/SSE acceptance (synthetic input only).

Run with the authorised idle physical acceptance-controller URL and --acceptance.
A simulation-arm callback is mandatory. No records or settings are erased.
Reads and real HTTP requests only; fixture reset is limited to this test's own
volatile race. No positive official-persistence or real-detector claim.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "stage15"))
from multiclient_http_acceptance import (
    EventStream, acquire_owner, arm_simulation, call, client, context, fixture,
    get_json, operation, passage, record_snapshot, require_authorised_target,
    restore_proposal, session_identity, set_proposal, start_simulated, state,
    wait_state, wait_until,
)


def main():
    require_authorised_target()
    master, master_jar = client()
    spectator, spectator_jar = client()
    assert call(master, "/health")[0] == 200
    original = get_json(master, "/proposal")
    assert state(master)["lifecycle"] == "READY" and original["activeLanes"] == 2
    assert isinstance(original["simulatedSession"], bool), original
    assert not context(master)["ownerConnected"], "Close existing controlling Browser first"
    context(spectator)
    assert len(list(master_jar)) == len(list(spectator_jar)) == 1
    assert list(master_jar)[0].value != list(spectator_jar)[0].value
    stream = EventStream(master, "stage13-controller").start()
    stream.discard_initial()
    acquire_owner(master, spectator)
    records = record_snapshot(master)
    assert call(spectator, "/bootstrap", "POST")[0] == 409
    assert operation(spectator, "/request/race-again", 12)["result"] == "REJECTED"
    assert call(master, "/request-result?correlationId=12")[0] == 204

    arm_simulation(master)
    duration = max(1, original["durationMinutes"])
    set_proposal(master, 10001, "LAP_RACE", 2, duration, 0)
    start_simulated(master, 1)
    passage(master, 1)
    passage(master, 2)
    passage(master, 1)
    finished = wait_state(master, "FINISHED")
    assert finished["resultSealed"] and finished["winningTime"] == finished["finishTime"]
    assert finished["historySequence"] == 0 and not finished["persistencePending"] and not finished["persistenceFault"], finished
    results = get_json(master, "/results")
    details = get_json(master, "/details")
    assert results["sealed"] and len(results["entries"]) == 2
    assert details["sealed"] and any(e["records"] for e in details["entries"])
    assert get_json(spectator, "/results") == results and get_json(spectator, "/details") == details
    assert call(master, "/fixture", "POST", {"action": "lane1"})[0] == 400
    assert get_json(master, "/results") == results
    assert record_snapshot(master) == records
    assert operation(spectator, "/request/race-again", 22)["result"] == "REJECTED"
    assert call(master, "/request-result?correlationId=22")[0] == 204

    assert operation(master, "/request/race-again", 2)["result"] == "ACCEPTED"
    wait_state(master, "READY")
    # Non-official live result is not reconstructed as Browser History on READY.
    # The engine may retain a sealed internal summary; the HTTP result boundary
    # must hide it after RACE AGAIN.
    assert not get_json(master, "/results")["sealed"]
    assert record_snapshot(master) == records
    old_ids = {e["raceEntryId"] for e in results["entries"]}
    arm_simulation(master)
    set_proposal(master, 10002, "LAP_RACE", 3, duration, 0)
    fresh = start_simulated(master, 3)
    assert fresh["lapTarget"] == 3 and not fresh["resultSealed"]
    assert all(e["laps"] == 0 for e in fresh["entries"])
    assert old_ids.isdisjoint(e["raceEntryId"] for e in fresh["entries"])
    assert context(spectator)["role"] == "Spectator"

    fixture(master, "reset")
    wait_state(master, "READY")
    clean = state(master)
    assert not clean["resultSealed"] and all(e["laps"] == 0 for e in clean["entries"])
    assert get_json(master, "/fact")["type"] == "NONE"
    arm_simulation(master)
    stream.close()
    wait_until(lambda: context(master), lambda c: not c["ownerConnected"], timeout=25)
    assert context(master)["isMaster"] is True
    assert operation(master, "/request/start", 4)["result"] == "REJECTED"
    assert state(master)["lifecycle"] == "READY"
    reconnected = EventStream(master, "stage13-reconnect").start()
    reconnected.discard_initial()
    wait_until(lambda: context(master), lambda c: c["ownerConnected"])
    start_simulated(master, 5)
    restore_proposal(master, original, 10003)
    assert record_snapshot(master) == records
    reconnected.close()
    print("Stage 13 real-controller HTTP/SSE acceptance PASS (simulated input only)")


if __name__ == "__main__":
    main()
