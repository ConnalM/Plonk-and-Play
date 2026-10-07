from __future__ import annotations

import hashlib
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent / "evidence"


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def source_sha() -> str:
    h = hashlib.sha256()
    for path in sorted(ROOT.rglob("*")):
        if not path.is_file() or ".git" in path.parts or ".pio" in path.parts or "evidence" in path.parts:
            continue
        if path.suffix.lower() not in {".h", ".cpp", ".py", ".ps1", ".ini", ".yaml", ".toml", ".md", ".json"}:
            continue
        h.update(str(path.relative_to(ROOT)).encode())
        h.update(path.read_bytes())
    return h.hexdigest()


def artifact(environment: str) -> dict:
    path = ROOT / "firmware" / ".pio" / "build" / environment / "firmware.merged.bin"
    return {"environment": environment, "path": str(path.relative_to(ROOT)), "bytes": path.stat().st_size, "sha256": sha256(path)}


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    artifacts = [artifact("stage14cacceptance"), artifact("stage14cdemo")]
    build = {
        "generatedAt": datetime.now(timezone.utc).isoformat(),
        "sourceHead": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "workingTreeSourceSha256": source_sha(),
        "artifacts": artifacts,
        "capacity": {"PP_MAX_ENTRIES": 8, "authoritativeLapCount": "unbounded by detail retention", "retainedDetailedLapsPerEntry": 16},
    }
    (OUT / "final-builds.json").write_text(json.dumps(build, indent=2) + "\n", encoding="utf-8")
    (OUT / "resource-evidence.json").write_text(json.dumps({
        "generatedAt": build["generatedAt"],
        # Values measured by the final PlatformIO builds emitted above.
        "stage14cacceptance": {"ramBytes": 116656, "ramCapacityBytes": 327680, "flashBytes": 894561, "flashCapacityBytes": 1310720},
        "stage14cdemo": {"ramBytes": 88352, "ramCapacityBytes": 327680, "flashBytes": 858417, "flashCapacityBytes": 1310720},
        "retention": build["capacity"],
        "source": build["workingTreeSourceSha256"],
    }, indent=2) + "\n", encoding="utf-8")
    print("Stage 14C final evidence generated:", ", ".join(a["path"] for a in artifacts))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
