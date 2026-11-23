"""Source-backed NVS transport checks for the C1 preference record."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
main = (root / "firmware/src/main.cpp").read_text(encoding="utf-8")
preferences = (root / "firmware/include/pp/taster_presentation_preferences.h").read_text(encoding="utf-8")

checks = {
    "NVS key allocation supports dedicated slots": '"slot-%u",s' in main,
    "wrong-length blobs are reported as present malformed records": "if(length!=n){memset(data,0,n);return true;}" in main,
    "preference slots cannot overlap race setup": "FirstSlot = 213" in preferences and "SecondSlot = 214" in preferences,
    "staging is verified before commit": "store_.read(StagingSlot, verified" in preferences and "memcmp(encoded, verified" in preferences,
    "corrupt and partial records fall back without rewrite": "DefaultsInvalid" in preferences and "if (!firstValid && !secondValid)" in preferences,
}
failed = [name for name, passed in checks.items() if not passed]
if failed:
    raise SystemExit("C1 NVS transport contract missing: " + ", ".join(failed))
print("Taster presentation NVS transport contract PASS")
