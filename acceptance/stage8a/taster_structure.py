from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[2]
diagram = json.loads((ROOT / "firmware/taster_wokwi/diagram.json").read_text())
env = (ROOT / "firmware/platformio.ini").read_text()
surface = (ROOT / "firmware/include/pp/taster_surface.h").read_text()
runtime = (ROOT / "firmware/include/pp/taster_runtime.h").read_text()

assert any(p["type"] == "board-esp32-s3-devkitc-1" for p in diagram["parts"])
assert any(p["type"] == "wokwi-ky-040" for p in diagram["parts"])
assert any(p["type"] == "chip-pp-taster-display" for p in diagram["parts"])
connections = {tuple(c[:2]) for c in diagram["connections"]}
for expected in (("esp:17", "display:IN"), ("encoder:CLK", "esp:5"),
                 ("encoder:DT", "esp:6"), ("encoder:SW", "esp:7")):
    assert expected in connections, expected
assert "[env:stage8atasterwokwi]" in env
assert "PP_STAGE8A_TASTER_WOKWI" in env
assert "class TasterAuthority" in surface
assert "class BrowserTasterAuthority" in (ROOT / "firmware/include/pp/taster_browser_adapter.h").read_text()
assert "submitStart(correlation, ClientContext::RaceDirectorSmug" in (ROOT / "firmware/include/pp/taster_browser_adapter.h").read_text()
for forbidden in ("raceStartedAt", "DEMO_FINISH_TIME", "sendStartLights", "enum TasterState"):
    assert forbidden not in surface + runtime
print("STAGE8A_TASTER_STRUCTURE PASS")
