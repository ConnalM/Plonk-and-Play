from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
browser = (ROOT / "firmware" / "include" / "pp" / "browser_interface.h").read_text(encoding="utf-8")
main = (ROOT / "firmware" / "src" / "main.cpp").read_text(encoding="utf-8")
embed = (ROOT / "firmware" / "browser" / "embed_normal.py").read_text(encoding="utf-8")
update = (ROOT / "firmware" / "include" / "pp" / "firmware_update.h").read_text(encoding="utf-8")

for route in ("/update/check-online", "/request/update/online", "/request/restart",
              "/request/reset-settings", "/request/erase-everything"):
    assert route in browser, f"missing production route {route}"
for marker in ("Restart P&amp;P", "Reset settings", "Erase everything",
               "Check for updates", "Download and prepare", "ERASE EVERYTHING"):
    assert marker in embed, f"missing customer maintenance UI marker {marker}"
for marker in ("context.controllingMaster", "context.lifecycleSettled",
               "history_.clear()", "records_.clearAll()", "memory_.resetToDefaults()",
               "memory_.erase()", "context.strongConfirmation", "customer records retained",
               "firmware/update capability retained"):
    assert marker in main, f"missing maintenance boundary {marker}"
for marker in ("class OnlineFirmwareProvider", "OnlineProviderUnavailable",
               "NoUpdateAvailable", "OnlineDownloadFailed", "DowngradeRejected"):
    assert marker in update, f"missing online update seam {marker}"
assert "Taster" not in embed, "maintenance controls leaked into Taster payload"
print("WP6_CUSTOMER_MAINTENANCE_CONTRACT_PASS")
