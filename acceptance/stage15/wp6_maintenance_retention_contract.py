"""Deterministic WP6 customer-maintenance contract proof.

This deliberately models only the product boundary: authority/lifecycle gates,
confirmation strength, and data-retention semantics. Physical reboot and OTA
boot-slot behaviour remain covered by their dedicated adapter tests.
"""

from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
main = (ROOT / "firmware" / "src" / "main.cpp").read_text(encoding="utf-8")
browser = (ROOT / "firmware" / "include" / "pp" / "browser_interface.h").read_text(encoding="utf-8")


@dataclass
class CustomerData:
    settings: dict
    history: list
    pb: int
    track: int
    era: int
    firmware: str = "known-good"
    update_capability: bool = True


def gate(controlling: bool, settled: bool, confirmed: bool, strong: bool = False):
    if not controlling:
        return "permission"
    if not settled:
        return "lifecycle"
    if not confirmed or (strong is False and strong is not None):
        return "confirmation"
    return "accepted"


def reset_settings(data: CustomerData, *, controlling=True, settled=True, confirmed=True):
    assert gate(controlling, settled, confirmed, None) == "accepted"
    data.settings = {"lanes": 2, "laps": 10, "mode": "LAP_RACE"}


def erase_everything(data: CustomerData, *, controlling=True, settled=True, confirmed=True, phrase=False):
    assert controlling and settled and confirmed and phrase
    data.settings = {"lanes": 2, "laps": 10, "mode": "LAP_RACE"}
    data.history.clear()
    data.pb = 0
    data.track = 0
    data.era += 1


data = CustomerData({"lanes": 4, "laps": 20, "mode": "ENDURANCE"}, [11, 10], 123, 123, 7)
original = (list(data.history), data.pb, data.track, data.era, data.firmware, data.update_capability)
assert gate(False, True, True) == "permission"
assert gate(True, False, True) == "lifecycle"
assert gate(True, True, False, None) == "confirmation"
reset_settings(data)
assert data.history == original[0] and (data.pb, data.track, data.era) == original[1:4]
assert (data.firmware, data.update_capability) == original[4:]

data = CustomerData({"lanes": 4}, [11, 10], 123, 123, 7)
assert not (data.history == [] or data.pb == 0)
erase_everything(data, phrase=True)
assert data.history == [] and data.pb == 0 and data.track == 0
assert data.settings["mode"] == "LAP_RACE"
assert data.firmware == "known-good" and data.update_capability

for marker in ("/request/restart", "/request/reset-settings", "/request/erase-everything",
               "context.controllingMaster", "context.lifecycleSettled", "context.strongConfirmation"):
    assert marker in browser or marker in main, marker
for marker in ("memory_.resetToDefaults()", "memory_.erase()", "history_.clear()", "records_.clearAll()"):
    assert marker in main, marker

print("WP6_MAINTENANCE_RETENTION_CONTRACT_PASS")
