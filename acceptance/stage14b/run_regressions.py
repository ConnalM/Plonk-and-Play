"""Run the applicable Stage 1-13 campaigns and bind their evidence to this tree."""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ACCEPTANCE = ROOT / "acceptance"
EVIDENCE = Path(__file__).resolve().parent / "evidence"
OUT = EVIDENCE / "regressions"


def source_hash() -> str:
    digest = hashlib.sha256()
    for path in sorted(ROOT.rglob("*")):
        if not path.is_file() or ".git" in path.parts or ".pio" in path.parts or "evidence" in path.parts:
            continue
        if path.suffix.lower() not in {".h", ".cpp", ".py", ".ps1", ".ini", ".yaml", ".toml", ".md", ".json"}:
            continue
        digest.update(str(path.relative_to(ROOT)).encode())
        digest.update(path.read_bytes())
    return digest.hexdigest()


def run_stage(number: int) -> dict:
    stage = ACCEPTANCE / f"stage{number}"
    script = stage / "run.ps1"
    if script.exists():
        # pwsh supplies the hashing cmdlets used by the historical acceptance
        # runners in this desktop environment.
        if number == 11:
            build = subprocess.run(["pwsh", "-NoProfile", "-File", str(ROOT / "firmware" / "build.ps1"), "-Environment", "stage11acceptance"], cwd=ROOT, text=True, capture_output=True, timeout=900, check=False)
            if build.returncode:
                return {"stage": number, "status": "FAIL", "returncode": build.returncode, "reason": "stage11 build failed", "build_output": build.stdout + build.stderr}
        command = ["pwsh", "-NoProfile", "-File", str(script)]
    else:
        evaluators = [stage / "evaluate.py", stage / "structural_review.py"]
        command = [sys.executable, str(evaluators[0])] if evaluators[0].exists() else None
        if command is None:
            return {"stage": number, "status": "SKIPPED", "reason": "no campaign entry point"}
    output_path = OUT / f"stage{number}.log"
    started = datetime.now(timezone.utc).isoformat()
    try:
        attempts=[]
        for attempt in range(1,4):
            result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, timeout=1800, check=False)
            attempts.append(f"ATTEMPT {attempt}\n{result.stdout}{result.stderr}")
            if result.returncode == 0:
                break
        output = "\n".join(attempts)
        if number == 11 and result.returncode == 0:
            # Stage 11's historical runner captures Wokwi output in its own
            # evidence files; include the evaluator/structural/stress results
            # in the Stage 14B-bound log so the manifest proves what ran.
            checks = []
            check_codes = []
            for checker in (stage / "evaluate.py", stage / "structural_review.py"):
                check = subprocess.run([sys.executable, str(checker)], cwd=ROOT, text=True, capture_output=True, check=False)
                checks.append(check.stdout + check.stderr)
                check_codes.append(check.returncode)
            stress = subprocess.run([sys.executable, str(stage / "stress_regression.py"), "--output", str(stage / "evidence" / "stress-regression-stage14b.jsonl")], cwd=ROOT, text=True, capture_output=True, check=False)
            checks.append(stress.stdout + stress.stderr)
            check_codes.append(stress.returncode)
            output += "\n" + "\n".join(checks)
            unexpected_failure = [item for item in checks if "FAIL" in item.replace("wrong=FAIL restored=PASS", "")]
            if unexpected_failure or any(code != 0 for code in check_codes):
                result = subprocess.CompletedProcess(result.args, 1, output, result.stderr)
        output_path.write_text(output, encoding="utf-8", errors="replace")
        return {
            "stage": number,
            "status": "PASS" if result.returncode == 0 else "FAIL",
            "returncode": result.returncode,
            "started": started,
            "log": str(output_path.relative_to(ROOT)),
            "log_sha256": hashlib.sha256(output.encode()).hexdigest(),
        }
    except subprocess.TimeoutExpired as error:
        output = (error.stdout or "") + (error.stderr or "") + "\nTIMEOUT\n"
        output_path.write_text(output, encoding="utf-8", errors="replace")
        return {"stage": number, "status": "FAIL", "returncode": None, "started": started, "log": str(output_path.relative_to(ROOT)), "log_sha256": hashlib.sha256(output.encode()).hexdigest(), "reason": "timeout"}


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    results = [run_stage(number) for number in range(1, 14)]
    manifest = {
        "campaign": "Stage 14B applicable Stage 1-13 regressions",
        "generatedAt": datetime.now(timezone.utc).isoformat(),
        "sourceCommit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "workingTreeSourceSha256": source_hash(),
        "results": results,
    }
    path = EVIDENCE / "regression-manifest.json"
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    failed = [row for row in results if row["status"] == "FAIL"]
    if failed:
        print("Stage 14B regression campaign FAIL:", ", ".join(f"Stage {row['stage']}" for row in failed))
        return 1
    print("Stage 14B regression campaign PASS: Stage 1-13 evidence captured")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
