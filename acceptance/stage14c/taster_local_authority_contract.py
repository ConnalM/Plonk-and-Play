"""WP8 S2: physical renderer is a presentation client only."""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[2]
ui = (root / "firmware/include/pp/wp7_physical_racing_ui.h").read_text()
adapter = (root / "firmware/include/pp/local_taster_authority.h").read_text()
main = (root / "firmware/src/main.cpp").read_text()
model = (root / "firmware/include/pp/taster_surface.h").read_text()
assert not re.search(r"\b(?:RaceControlModule|Noticeboard&|Bus::Endpoint|bus_|control_|noticeboard_|presentationPreferences_|TasterPresentationPreferencesController)\b", ui)
for marker in ("authority_.stateAt(now)", "authority_.lifecycle()", "authority_.pollResults()",
               "authority_.committedPreferences()", "surface_.start()",
               "surface_.operation(", "surface_.commitAdjustment()"):
    assert marker in ui, marker
assert "authority_.submitOperation(" not in ui
assert "surface_.allocateCorrelation()" not in ui
for op in ("Resume", "EndRace", "Pause", "RaceAgain"):
    assert f"publishOperation(SessionOperation::{op}," in ui, op
assert 'publishOperation(SessionOperation::EndRace, true,' in ui
assert 'ClientContext::StandaloneTaster' in adapter
assert 'wp7PhysicalUi(boardDisplay,localTasterAuthority)' in main
assert 'wp7PhysicalUi.submit' not in main
assert model.count('pendingCorrelation_ = allocateCorrelation();') == 3
print("S2 physical UI boundary source contract PASS")
