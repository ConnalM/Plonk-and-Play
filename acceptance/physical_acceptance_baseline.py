"""Immutable proposal baseline for physical acceptance suites.

Capture this file, read-only, immediately before a Stage 15/13/8 suite:

    python physical_acceptance_baseline.py capture http://<taster> --acceptance baseline.json

Then set PP_ACCEPTANCE_BASELINE to that new file for every stage.  The file is
created once and is never updated by a stage.  A stage therefore refuses to
mistake a setting left by an interrupted predecessor for the customer's
starting configuration.
"""

from __future__ import annotations

import json
import os
import sys
from pathlib import Path
from urllib.request import ProxyHandler, Request, build_opener


PROPOSAL_FIELDS = (
    "mode", "lapTarget", "durationMinutes", "finishBehaviour",
    "activeLanes", "simulatedSession", "entries",
)


def proposal_identity(proposal):
    return tuple(proposal[field] for field in PROPOSAL_FIELDS)


def load_baseline(path):
    try:
        baseline = json.loads(path.read_text(encoding="utf-8"))
        expected = baseline["proposal"]
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise AssertionError(f"invalid physical-acceptance baseline {path}: {error}") from error
    assert isinstance(baseline, dict) and baseline.get("schema") == 1 and isinstance(expected, dict), baseline
    return expected


def require_immutable_baseline(observed):
    """Return the pre-suite proposal or stop before a stage can alter it."""
    value = os.environ.get("PP_ACCEPTANCE_BASELINE")
    if not value:
        raise AssertionError(
            "PP_ACCEPTANCE_BASELINE is required; capture an immutable, read-only "
            "proposal baseline before physical acceptance."
        )
    expected = load_baseline(Path(value))
    if proposal_identity(observed) != proposal_identity(expected):
        raise AssertionError(
            "proposal differs from immutable pre-suite baseline; do not run or "
            "overwrite settings after an interrupted acceptance stage"
        )
    return expected


def _read_proposal(base):
    opener = build_opener(ProxyHandler({}))
    request = Request(base.rstrip("/") + "/proposal", headers={"Connection": "close"})
    with opener.open(request, timeout=5) as response:
        assert response.status == 200, response.status
        result = json.loads(response.read().decode())
    assert isinstance(result, dict) and proposal_identity(result), result
    return result


def capture(base, destination):
    path = Path(destination)
    if path.exists():
        raise AssertionError(f"baseline already exists; will not overwrite {path}")
    proposal = _read_proposal(base)
    path.parent.mkdir(parents=True, exist_ok=True)
    # Exclusive create ensures a second process cannot replace the evidence.
    with path.open("x", encoding="utf-8") as output:
        json.dump({"schema": 1, "proposal": proposal}, output, indent=2, sort_keys=True)
        output.write("\n")
    print(f"Captured immutable physical-acceptance baseline: {path}")


def verify(base, source):
    expected = load_baseline(Path(source))
    observed = _read_proposal(base)
    if proposal_identity(observed) != proposal_identity(expected):
        raise AssertionError(
            "proposal restoration differs from immutable pre-suite baseline; "
            "stop for review without overwriting settings"
        )
    print(f"Verified proposal restoration against immutable baseline: {source}")


if __name__ == "__main__":
    if len(sys.argv) != 5 or sys.argv[1] not in ("capture", "verify") or sys.argv[3] != "--acceptance" or not sys.argv[2].startswith("http://"):
        raise SystemExit("usage: physical_acceptance_baseline.py {capture|verify} http://<taster> --acceptance <baseline.json>")
    if sys.argv[1] == "capture":
        capture(sys.argv[2], sys.argv[4])
    else:
        verify(sys.argv[2], sys.argv[4])
