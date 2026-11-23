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
    # Step B2 deliberately made this V21 view read-only.  It must display the
    # controller's actual detector facts, not revive the superseded setup form.
    "Track Setup no longer has placeholder-only behavior": "Track Setup" in browser and "G('/track/setup')" in browser and "b2TrackView" in browser,
    "Browser reads authoritative detector route": "/track/setup" in browser and "trackSetupRoute" in interface,
    # Step B2 uses its accepted b2-prefixed cache. Later settings increments
    # may append a separate script, but the Track view must still be embedded.
    "embedded Track Setup extension executes inside the normal page": "let b2TrackData=" in embedded and "function b2TrackView()" in embedded,
    "Track Setup remains read-only while controller assignment support remains available": "applyTrackSetup" not in browser and "setupAssignmentMask" in interface and "setupLane1" in interface,
    "Browser gates editing on controlling READY state": "C?.isMaster===true&&S?.lifecycle==='READY'" in browser,
    "Race Control validates assignments and provider health": "requiredDetectorCapabilitiesHealthy(candidate, *detectorProvider_)" in control and "RequestRejection::RequiredCapabilityUnavailable" in control,
    "Track Setup persistence is synchronous and failure-rejecting": "trackConfigurationPersistence_->save(configuration)" in control and "RequestRejection::StorageUnavailable" in control and "persistTrackConfigurationIfChanged" not in main,
    "Track configuration stages outside the authoritative namespace": "class TrackConfigurationMemory" in track and "StagingSlot = 4" in track and "store_.writeUnreferenced(StagingSlot" in track and "store_.read(StagingSlot" in track and "store_.writeMetadata(target" in track and "store_.read(target" not in track,
    "Track configuration is wired into production startup": "trackConfigurationMemory.load" in main and "&trackConfigurationMemory" in main,
    "Input and Race Engine boundaries remain unchanged": "input.tick(systemTime())" in main and "raceEngine.tick()" in main,
}
for name, passed in checks.items():
    print(("PASS " if passed else "FAIL ") + name)
if not all(checks.values()):
    sys.exit(1)
print("WP7.3 Track Setup structural review PASS")
