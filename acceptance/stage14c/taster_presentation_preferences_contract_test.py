"""Source contract checks for WP8 Step C1; no firmware build or device access."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
preferences = (root / "firmware/include/pp/taster_presentation_preferences.h").read_text(encoding="utf-8")
browser = (root / "firmware/include/pp/browser_interface.h").read_text(encoding="utf-8")
renderer = (root / "firmware/include/pp/wp7_physical_racing_ui.h").read_text(encoding="utf-8")
page = (root / "firmware/browser/normal_browser.html").read_text(encoding="utf-8")
main = (root / "firmware/src/main.cpp").read_text(encoding="utf-8")

required = {
    "separate slots": "FirstSlot = 213" in preferences and "SecondSlot = 214" in preferences and "StagingSlot = 215" in preferences,
    "versioned A/B record": "Magic = 0x31505054u" in preferences and "FormatVersion = 1" in preferences,
    "factory defaults": '"LANE 1"' in preferences and '"LANE 2"' in preferences,
    "built-in graphics only": "enum class TasterCarGraphic" in preferences and "GreenMgb" in preferences and "RedEtype" in preferences,
    "controller guards": "PermissionDenied" in preferences and "LifecycleNotReady" in preferences,
    "read route": '"/taster/preferences"' in browser,
    "update route": '"/request/taster/preferences"' in browser,
    "factory route": '"/request/taster/preferences/restore-factory"' in browser,
    "renderer committed values": "presentationPreferences_->current()" in renderer and "laneCar(uint8_t lane)" in renderer,
    "main loads before use": "tasterPresentationPreferences.load()" in main,
    "real settings controls": "SAVE TASTER" in page and "RESTORE FACTORY TASTER" in page,
    "view-only guard": "Only the controlling Race Director may change the Taster" in page,
    "unavailable display state": "Taster presentation unavailable" in page,
    "no browser-local persistence": "localStorage" not in page and "sessionStorage.setItem('c1" not in page,
    "audio remains deferred": "Audio preferences require the separate C2 playback implementation." in page,
}
failed = [name for name, passed in required.items() if not passed]
if failed:
    raise SystemExit("C1 contract missing: " + ", ".join(failed))
print("Taster presentation preferences contract PASS")
