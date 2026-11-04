from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
platformio = (ROOT / "firmware" / "platformio.ini").read_text(encoding="utf-8")
embed = (ROOT / "firmware" / "browser" / "embed_normal.py").read_text(encoding="utf-8")
main = (ROOT / "firmware" / "src" / "main.cpp").read_text(encoding="utf-8")
control = (ROOT / "firmware" / "include" / "pp" / "race_control.h").read_text(encoding="utf-8")
page = (ROOT / "firmware" / "include" / "pp" / "normal_browser_page.inc").read_text(encoding="utf-8")

stage8_line = next(line for line in platformio.splitlines() if line.startswith("build_flags = -Wall") and "PP_PHASE8B_REAL_SD" in line)
assert "PP_SIMULATED_RACING_TEST" not in stage8_line, "production stage8 must not enable simulation"
assert "[env:stage8bp1s3sd-simulated-racing]" in platformio
assert "-DPP_SIMULATED_RACING_TEST=1" in platformio
assert "if simulated_racing_ui:" in embed
assert "KEYBOARD RACE · TEST ONLY" in embed
assert "PASS L1 [1]" in embed and "PASS L2 [2]" in embed
assert "armSimulatedSession" in control and "simulatedSessionActive" in control
assert "#if defined(PP_SIMULATED_RACING_TEST)" in main

# The checked-in/generated production payload must remain free of the test drawer.
assert 'id="devDrawer"' not in page
assert "PASS L1 [1]" not in page
print("Simulated Browser production-gating contract PASS")
