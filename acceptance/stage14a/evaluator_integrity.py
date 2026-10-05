"""Genuine evaluator-integrity check for Stage 14A.T.

The check copies the governing firmware headers into a temporary tree, makes a
real two-entry regression in the copied Race Engine capacity declaration, and
requires the structural evaluator to reject that tree.  It then removes the
corruption and requires the same evaluator to accept the repository tree.
"""
from __future__ import annotations

import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EVIDENCE = Path(__file__).resolve().parent / "evidence"
HEADERS = ["core.h", "session_definition.h", "race_engine.h", "noticeboard.h", "record_store.h", "browser_interface.h"]


def tree_hash() -> str:
    digest = hashlib.sha256()
    for name in HEADERS:
        path = ROOT / "firmware" / "include" / "pp" / name
        digest.update(name.encode())
        digest.update(path.read_bytes())
    return digest.hexdigest()


def evaluate(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(Path(__file__).with_name("structural_review.py")), "--root", str(root)],
        text=True,
        capture_output=True,
        check=False,
    )


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="stage14a-evaluator-") as temporary:
        temporary_root = Path(temporary)
        destination = temporary_root / "firmware" / "include" / "pp"
        destination.mkdir(parents=True)
        for name in HEADERS:
            shutil.copy2(ROOT / "firmware" / "include" / "pp" / name, destination / name)
        corrupted = destination / "race_engine.h"
        source = corrupted.read_text(encoding="utf-8")
        marker = "MaxEntries=PP_MAX_ENTRIES"
        if marker not in source:
            print("Stage 14A evaluator integrity FAIL: corruption marker missing", file=sys.stderr)
            return 1
        corrupted.write_text(source.replace(marker, "MaxEntries=2", 1), encoding="utf-8")
        failed = evaluate(temporary_root)
        corrupted_detected = failed.returncode != 0 and "engine_not_two" in failed.stdout
    restored = evaluate(ROOT)
    restored_passed = restored.returncode == 0
    record = {
        "corrupted_condition": "RaceEngine MaxEntries forced to 2 in an isolated copy",
        "corrupted_evaluator": "FAIL" if corrupted_detected else "UNEXPECTED PASS",
        "restored_evaluator": "PASS" if restored_passed else "FAIL",
        "source_hash": tree_hash(),
        "corrupted_output": failed.stdout.strip(),
        "restored_output": restored.stdout.strip(),
    }
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / "evaluator-integrity.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    if not corrupted_detected or not restored_passed:
        print("Stage 14A evaluator integrity FAIL")
        return 1
    print("Stage 14A evaluator integrity PASS: corrupted=FAIL restored=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
