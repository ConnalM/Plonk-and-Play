from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
browser = (root / "firmware/browser/normal_browser.html").read_text(encoding="utf-8")
interface = (root / "firmware/include/pp/browser_interface.h").read_text(encoding="utf-8")
control = (root / "firmware/include/pp/race_control.h").read_text(encoding="utf-8")
track = (root / "firmware/include/pp/track_configuration.h").read_text(encoding="utf-8")
main = (root / "firmware/src/main.cpp").read_text(encoding="utf-8")
embedded = (root / "firmware/include/pp/normal_browser_page.inc").read_text(encoding="utf-8")

checks = {
    "Track Setup no longer has placeholder-only behavior": "Track Setup" in browser and "Reading detector capabilities" in browser and "applyTrackSetup" in browser,
    "Browser reads authoritative detector route": "/track/setup" in browser and "trackSetupRoute" in interface,
    "embedded Track Setup extension executes inside the primary script": "let trackSetupData=" in embedded and embedded.count("<script>") == 1 and embedded.count("</script>") == 1 and embedded.index("let trackSetupData=") < embedded.index("</script>"),
    "Browser submits lane assignments through existing setup route": "lane1Device" in browser and "setupAssignmentMask" in interface and "setupLane1" in interface,
    "Browser gates editing on controlling READY state": "C?.isMaster===true&&S?.lifecycle==='READY'" in browser,
    "Race Control validates assignments and provider health": "requiredDetectorCapabilitiesHealthy(candidate, *detectorProvider_)" in control and "RequestRejection::RequiredCapabilityUnavailable" in control,
    "Track configuration uses existing Store boundary": "class TrackConfigurationMemory" in track and "store_.write(target" in track and "store_.read(target" in track,
    "Track configuration is wired into production startup": "trackConfigurationMemory.load" in main and "persistTrackConfigurationIfChanged" in main,
    "Input and Race Engine boundaries remain unchanged": "input.tick(systemTime())" in main and "raceEngine.tick()" in main,
}
for name, passed in checks.items():
    print(("PASS " if passed else "FAIL ") + name)
if not all(checks.values()):
    sys.exit(1)
print("WP7.3 Track Setup structural review PASS")
