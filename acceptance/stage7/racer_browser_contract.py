from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[2]
EMBED = (ROOT / "firmware/browser/embed_normal.py").read_text(encoding="utf-8")
INTERFACE = (ROOT / "firmware/include/pp/browser_interface.h").read_text(encoding="utf-8")
CONTROL = (ROOT / "firmware/include/pp/race_control.h").read_text(encoding="utf-8")
GENERATED = (ROOT / "firmware/include/pp/normal_browser_page.inc").read_text(encoding="utf-8")


checks = {
    "Browser has a normal Racers view": "function racerSetupView()" in EMBED,
    "Browser reads authoritative racer identities": "G('/racers')" in EMBED and "racersRoute" in INTERFACE,
    "Browser uses real proposal selections": "P?.entries?.[0]?.mugId" in EMBED and "proposalRevision" in INTERFACE,
    "Browser submits through the existing setup request": "fetch('/request/setup'" in EMBED and "racerAssignmentMask" in INTERFACE,
    "Browser gates edits on the controlling READY Race Director": "C?.isMaster===true&&S?.lifecycle==='READY'" in EMBED,
    "Browser prevents duplicate lane selection": "lane1===lane2" in EMBED and "assigned to other lane" in EMBED,
    "Browser creates named MUGs through controller authority": "fetch('/request/mug/create'" in EMBED and "mugCreateRoute" in INTERFACE and "createMug" in INTERFACE,
    "Browser renders actual saved MUG names": "racerEscape(r.name" in EMBED and "catalog->find" in INTERFACE,
    "Browser clearly reports catalog failures": "STORAGE_UNAVAILABLE" in EMBED and "INVALID_NAME" in EMBED,
    "Controller validates racer identity separately from detectors": "setupRacerAssignmentMask" in CONTROL and "racerSelectionPersistence_" in CONTROL,
    "Controller rejects unknown or archived identities": "validMugIdentity" in CONTROL and "find(id, entry, false)" in CONTROL,
    "Generated payload includes the Racers view": "baseRenderWithRacers" in GENERATED and "Apply racers" in GENERATED,
    "Generated payload includes the authoritative route": "/racers" in GENERATED,
    "Generated payload includes named MUG creation": "/request/mug/create" in GENERATED and "Create named MUG" in GENERATED,
}

for name, passed in checks.items():
    print(("PASS " if passed else "FAIL ") + name)

if not all(checks.values()):
    sys.exit(1)

print("WP7.3 Racer Browser contract PASS")
