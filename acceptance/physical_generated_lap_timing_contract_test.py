"""Guard the real-time physical-fixture minimum lap interval."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
stage15 = (ROOT / "acceptance/stage15/multiclient_http_acceptance.py").read_text(encoding="utf-8")
stage8 = (ROOT / "acceptance/stage8/physical_taster_simulation_acceptance.py").read_text(encoding="utf-8")
stage13 = (ROOT / "acceptance/stage13/http_acceptance.py").read_text(encoding="utf-8")

assert "MIN_GENERATED_LAP_SECONDS = 1.05" in stage15
assert "time.sleep(MIN_GENERATED_LAP_SECONDS)" in stage15

passage = stage15[stage15.index("def passage("):stage15.index("\ndef session_identity", stage15.index("def passage("))]
assert passage.index("wait_for_generated_lap_interval()") < passage.index('fixture(owner, f"lane{lane}")')

assert "wait_for_generated_lap_interval" in stage8
taster_pass = stage8[stage8.index("def taster_pass("):stage8.index("\ndef restore_taster_proposal", stage8.index("def taster_pass("))]
assert taster_pass.index("wait_for_generated_lap_interval()") < taster_pass.index('fixture(opener, f"taster-lane{lane}")')

# The Endurance wall-clock remains authoritative: its expiry waits are not
# replaced by lap delays, and Complete Current Lap still injects only after
# expiry.  The rejected post-finish fixture remains immediate and unchanged.
endurance = stage8[stage8.index("def endurance_witness("):stage8.index("\ndef standalone_handover_witness", stage8.index("def endurance_witness("))]
assert 'wait_state(opener, "FINISHED", timeout=80)' in endurance
assert 'lambda state: state["durationExpired"] is True, timeout=80' in endurance
assert endurance.index('lambda state: state["durationExpired"] is True') < endurance.index("taster_pass(opener, 1, dwell=0.75)", endurance.index('lambda state: state["durationExpired"] is True'))
assert 'fixture(master, "lane1", accepted=False)' in stage13

print("Physical generated-lap timing contract PASS: >=1.05s fixture laps; Endurance expiry and rejected-input paths retained")
