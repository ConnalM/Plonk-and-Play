from pathlib import Path
import json,subprocess,sys
root=Path(__file__).resolve().parents[2]
docs=(root/'docs/ACCEPTANCE_TESTS_STAGE_14A.md').read_text(encoding='utf-8')
required=['Stage 1–13','four-entry','Race Again','History','PB','Track Record','does not authorize']
missing=[x for x in required if x not in docs]
if missing: print('Stage 14A regression contract FAIL:',', '.join(missing));sys.exit(1)
manifest=root/'acceptance/stage14a/evidence/regression-manifest.json'
try:
    value=json.loads(manifest.read_text(encoding='utf-8'))
    rows=value.get('results',[])
    if len(rows)!=13 or any(row.get('status')!='PASS' for row in rows) or not value.get('workingTreeSourceSha256'):
        raise ValueError('incomplete regression manifest')
except Exception as error:
    print('Stage 14A regression evidence FAIL:',error);sys.exit(1)
print('Stage 14A regression evidence PASS: Stage 1–13 campaigns bound to current source')
