from pathlib import Path
import sys

evidence = Path(__file__).parent / "evidence"
serial = next((p for p in (evidence/"serial-latest.txt", evidence/"serial-complete-attempt2.txt", evidence/"serial-grid.txt", evidence/"serial-current.txt") if p.exists()), None)
if serial is None:
    print("missing serial evidence"); sys.exit(1)
text = serial.read_text(errors="replace")
required = ["test=11.1", "test=11.2", "test=11.3", "test=11.4", "test=11.5", "test=11.6", "test=11.7", "test=11.9", "test=11.10", "test=11.11", "test=11.12 facts_state=1 resumed_fact=1", "regression=repeat_pause_restart pass=1", "regression=reset_types browser_keeps_master=1 full_clears_master=1 ready=1 fact_clean=1 start_after_reset=1", "test=11.18", "test=11.19", "test=11.20", "test=11.T", "ACC DONE"]
missing = [x for x in required if x not in text]
if missing:
    print("missing evidence:", ", ".join(missing)); sys.exit(1)
if "wrong=FAIL restored=PASS" not in text:
    print("missing deliberate evaluator-failure evidence"); sys.exit(1)
print("Stage 11 smoke/evidence evaluator PASS")
