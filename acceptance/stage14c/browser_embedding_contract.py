"""Prove the checked-in Browser payload is regenerated from its sole source."""

from pathlib import Path
import os
import runpy


ROOT = Path(__file__).resolve().parents[2]
FIRMWARE = ROOT / "firmware"
SOURCE = FIRMWARE / "browser" / "normal_browser.html"
GENERATED = FIRMWARE / "include" / "pp" / "normal_browser_page.inc"
GENERATOR = FIRMWARE / "browser" / "embed_normal.py"


class GeneratorEnvironment:
    def subst(self, value):
        return str(FIRMWARE) if value == "$PROJECT_DIR" else value

    def get(self, _key, default=None):
        return default

    def Append(self, **_kwargs):
        pass

    def Prepend(self, **_kwargs):
        pass


before = GENERATED.read_bytes()
old_cwd = Path.cwd()
try:
    os.chdir(FIRMWARE)
    runpy.run_path(str(GENERATOR), init_globals={
        "env": GeneratorEnvironment(),
        "Import": lambda _name: None,
    })
finally:
    os.chdir(old_cwd)
after = GENERATED.read_bytes()

assert before == after, "normal_browser_page.inc was stale; regeneration changed it"
source = SOURCE.read_text(encoding="utf-8")
generated = after.decode("utf-8")
for marker in (
    "V21 presentation shell",
    "Primary navigation",
    "Timed Stage and Drag require their own authoritative race-engine increments.",
    "Audio preferences and the auxiliary display editor are deferred.",
):
    assert marker in source and marker in generated, f"missing generated V21 marker: {marker}"

print("Stage 14C Browser embedding contract PASS: regenerated payload is byte-identical")
