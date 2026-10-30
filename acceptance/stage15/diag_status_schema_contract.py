"""Static contract for the authoritative /diag/status health split."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BROWSER = (ROOT / "firmware/include/pp/browser_interface.h").read_text(encoding="utf-8")
MAIN = (ROOT / "firmware/src/main.cpp").read_text(encoding="utf-8")
EMBED = (ROOT / "firmware/browser/embed_normal.py").read_text(encoding="utf-8")

assert '\\"physicalStorage\\":\\"%s\\"' in BROWSER
assert '\\"historyHealth\\":\\"%s\\"' in BROWSER
assert 'physicalStorageText(browser->physicalOwner_)' in BROWSER
assert 'historyHealthText(browser->history_)' in BROWSER
assert "PhysicalSdOwner* physicalOwner_" in BROWSER
assert "&physicalSdOwner" in MAIN
assert 'PhysicalSdOwner::Health::Mounted ? "HEALTHY" : "UNAVAILABLE"' in BROWSER
assert 'case StoreHealth::Healthy: return "HEALTHY"' in BROWSER
assert 'case StoreHealth::Corrupt: return "CORRUPT"' in BROWSER
assert 'case StoreHealth::Unsupported: return "UNSUPPORTED"' in BROWSER
assert 'case StoreHealth::Unavailable: return "UNAVAILABLE"' in BROWSER
assert "status?.physicalStorage" in EMBED
assert "status?.historyHealth" in EMBED
assert "const physical=String(status?.physicalStorage" in EMBED

print("PASS: /diag/status exposes physicalStorage and historyHealth")
print("PASS: physical and logical health use separate authoritative sources")
print("PASS: embedded Browser consumes the split schema")
