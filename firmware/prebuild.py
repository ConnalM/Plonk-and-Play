"""Run the normal Browser embed and build provenance generators together."""
from pathlib import Path

Import("env")
root = Path(env["PROJECT_DIR"])
for script in (root / "browser" / "embed_normal.py", root / "build_identity.py"):
    source = script.read_text(encoding="utf-8")
    exec(compile(source, str(script), "exec"), globals(), globals())
