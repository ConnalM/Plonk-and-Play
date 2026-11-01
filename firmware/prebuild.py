"""Run the normal Browser embed and build provenance generators together."""
from pathlib import Path
import os

Import("env")
root = Path(env["PROJECT_DIR"])
public_key = os.environ.get("PP_UPDATE_PUBLIC_KEY_HEX", "")
if public_key:
    if len(public_key) & 1 or any(char not in "0123456789abcdefABCDEF" for char in public_key):
        raise RuntimeError("PP_UPDATE_PUBLIC_KEY_HEX must be hexadecimal DER public-key bytes")
    # The value is public and deliberately supplied by the protected build
    # environment. The production private signing key never enters SCons.
    env.Append(CPPDEFINES=[("PP_UPDATE_PUBLIC_KEY_HEX", '\\"' + public_key + '\\"')])
for script in (root / "browser" / "embed_normal.py", root / "build_identity.py"):
    source = script.read_text(encoding="utf-8")
    exec(compile(source, str(script), "exec"), globals(), globals())
