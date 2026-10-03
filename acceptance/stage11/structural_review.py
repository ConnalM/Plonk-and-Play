from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[2]
checks = {
    "new_types_appended": "SessionOperationRequest=11" in (ROOT/"firmware/include/pp/core.h").read_text(),
    "settlement_bus_path": all(s in (ROOT/"firmware/src/main.cpp").read_text() for s in ["Type::SessionOperation", "Type::InputSettlement", "Type::PauseSettled"]),
    "no_browser_bus_participant": "bus.attach" not in (ROOT/"firmware/include/pp/browser_interface.h").read_text() and "bus.subscribe" not in (ROOT/"firmware/include/pp/browser_interface.h").read_text(),
    "relevant_time_boundaries": all(s in (ROOT/"firmware/include/pp/race_engine.h").read_text() for s in ["pauseAt_", "restartAt_", "relevantTime"]),
    "fixed_restart_interval": "3000000" in (ROOT/"firmware/include/pp/race_control.h").read_text(),
    "browser_controls": all(s in (ROOT/"firmware/include/pp/browser_interface.h").read_text() for s in ["/request/pause", "/request/honour-restart", "/request/grid-restart"]),
}
if not all(checks.values()):
    print(json.dumps(checks, indent=2)); raise SystemExit(1)
print(json.dumps(checks, indent=2))
