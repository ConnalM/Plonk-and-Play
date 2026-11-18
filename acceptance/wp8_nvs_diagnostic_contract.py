from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
platformio = (ROOT / "firmware" / "platformio.ini").read_text(encoding="utf-8")
main = (ROOT / "firmware" / "src" / "main.cpp").read_text(encoding="utf-8")

normal = re.search(
    r"\[env:stage8bp1s3sd-wp7-physical-racing-test\](.*?)(?=\n\[env:|\Z)",
    platformio,
    re.S,
)
diagnostic = re.search(
    r"\[env:stage8bp1s3sd-wp8-nvs-diagnostic\](.*?)(?=\n\[env:|\Z)",
    platformio,
    re.S,
)
assert normal and diagnostic, "physical and diagnostic environments must exist"
assert "-DPP_WP8_NVS_DIAGNOSTIC" not in normal.group(1)
assert "-DPP_WP8_NVS_DIAGNOSTIC=1" in diagnostic.group(1)

assert "ramMode_ = mode" in main
assert "ramLaps_ = laps" in main
assert "ramDurationMinutes_ = durationMinutes" in main
assert "const uint32_t nvsWrites = storage.writeCalls - writesBefore;" in main
assert "delegate_.saveRaceSettings(mode, laps, durationMinutes)" in main

print("WP8_NVS_DIAGNOSTIC_CONTRACT_PASS")
