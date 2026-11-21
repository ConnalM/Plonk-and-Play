"""Host regression for immutable physical-acceptance proposal baselines."""

from __future__ import annotations

import os
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0, str(ROOT / "acceptance"))
from physical_acceptance_baseline import proposal_identity, require_immutable_baseline


baseline = {
    "mode": "ENDURANCE", "lapTarget": 943, "durationMinutes": 1,
    "finishBehaviour": "COMPLETE_CURRENT_LAP", "activeLanes": 2,
    "simulatedSession": True, "entries": [],
}
temporary = {
    **baseline, "mode": "LAP_RACE", "lapTarget": 2,
    "finishBehaviour": "IMMEDIATE",
}

with tempfile.TemporaryDirectory() as directory:
    baseline_path = Path(directory) / "baseline.json"
    baseline_path.write_text('{"schema": 1, "proposal": ' + __import__("json").dumps(baseline) + "}\n", encoding="utf-8")
    previous = os.environ.get("PP_ACCEPTANCE_BASELINE")
    os.environ["PP_ACCEPTANCE_BASELINE"] = str(baseline_path)
    try:
        # A clean stage admits the captured proposal and restores precisely it.
        assert proposal_identity(require_immutable_baseline(baseline)) == proposal_identity(baseline)
        # An interrupted predecessor leaves a temporary two-lap proposal.  The
        # successor must stop, not promote that state into a new baseline.
        try:
            require_immutable_baseline(temporary)
            raise AssertionError("interrupted-stage proposal was accepted as a baseline")
        except AssertionError as error:
            assert "differs from immutable" in str(error)
    finally:
        if previous is None:
            os.environ.pop("PP_ACCEPTANCE_BASELINE", None)
        else:
            os.environ["PP_ACCEPTANCE_BASELINE"] = previous

for stage in (
    ROOT / "acceptance/stage15/multiclient_http_acceptance.py",
    ROOT / "acceptance/stage13/http_acceptance.py",
    ROOT / "acceptance/stage8/physical_taster_simulation_acceptance.py",
):
    source = stage.read_text(encoding="utf-8")
    assert "require_immutable_baseline" in source, stage
    assert "original = require_immutable_baseline" in source, stage

runner = (ROOT / "acceptance/run_physical_wp8_02_verification.ps1").read_text(encoding="utf-8")
assert "proposal-baseline.json" in runner
assert "physical_acceptance_baseline.py') capture" in runner
assert "physical_acceptance_baseline.py') verify" in runner
assert runner.index("-Name 'stage15'") < runner.index("-Name 'stage13'") < runner.index("-Name 'stage8'")
assert "Evidence directory already exists; refusing to overwrite" in runner

print("Physical acceptance baseline contract PASS: immutable pre-suite state rejects interrupted-stage and handover residue")
