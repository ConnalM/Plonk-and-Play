from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[2]
docs=(root/'docs/ACCEPTANCE_TESTS_STAGE_14A.md').read_text(encoding='utf-8')
required=['Stage 1–13','four-entry','Race Again','History','PB','Track Record','does not authorize']
missing=[x for x in required if x not in docs]
if missing: print('Stage 14A regression contract FAIL:',', '.join(missing));sys.exit(1)
print('Stage 14A regression contract PASS: Stage 1–13 coverage declared')
