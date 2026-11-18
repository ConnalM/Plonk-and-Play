from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
main = (ROOT / "firmware/src/main.cpp").read_text(encoding="utf-8")
ui = (ROOT / "firmware/include/pp/wp7_physical_racing_ui.h").read_text(encoding="utf-8")

# The physical target starts the existing panel/UI before the long local
# service/bootstrap sequence, and performs no second display initialisation
# when LoadConfiguration is acknowledged.
early_display = main.index("wp7DisplayReady = boardDisplay.begin();")
storage_begin = main.index("storage.begin();")
assert early_display < storage_begin
assert main.index("wp7PhysicalUi.begin();", early_display) < storage_begin
config_block = main.index("wp7PhysicalUi.completeStartup(ready && wp7DisplayReady);")
assert config_block > storage_begin
assert main.count("wp7DisplayReady = boardDisplay.begin();") == 1
assert main.count("wp7PhysicalUi.begin();") == 1

# A startup-complete gate, rather than a time delay, controls the transition.
assert "startupComplete_ = false;" in ui
assert "welcomeCanLeaveAfterMinimum(startupComplete_" in ui
assert "kWelcomeMinimumMs = 3000" in ui
assert "welcome first presented" in ui
assert "welcome exited" in ui
assert "updateWelcome(systemTime(), millis(), true);" not in ui

# Customer-facing startup text is intentionally stable. Technical stage names
# remain serial diagnostics and are not cycled beneath the welcome bar.
assert 'welcomeCustomerStatus(false)' in ui
assert '"WAITING FOR DETECTORS"' not in ui
assert '"CHECKING RACE SETUP"' not in ui
assert '"WAITING FOR P&P"' not in ui

# The progress/status path updates only its own welcome regions after the
# first frame; the static logo, title and strapline are not rebuilt each tick.
assert "const bool fullRedraw = !welcomeDrawn_;" in ui
assert "gfx->fillRect(barX, barY, barWidth, barHeight, kWelcomeBackground);" in ui
assert "gfx->fillRect(statusX, statusY, statusWidth, statusHeight," in ui

print("WP7_WELCOME_STARTUP PASS")
