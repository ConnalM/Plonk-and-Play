from pathlib import Path
import json,re,sys
root=Path(__file__).parent
text=(root/'evidence'/'serial.txt').read_text(errors='replace') if (root/'evidence'/'serial.txt').exists() else ''
ids=[*(f'14B.{i}' for i in range(1,30)),'14B.T']
failed=[]
for ident in ids:
    rows=re.findall(rf'ACC S14B test={re.escape(ident)}[^\r\n]*',text)
    if not rows or not any('pass=1' in row for row in rows): failed.append(ident)
if 'pass=0' in text or 'ACC DONE' not in text: failed.append('campaign')
try:
    proof=json.loads((root/'evidence'/'evaluator-integrity.json').read_text())
    if proof.get('corrupted_evaluator')!='FAIL' or proof.get('restored_evaluator')!='PASS': failed.append('14B.29')
except Exception: failed.append('14B.29')
if failed:
    print('Stage 14B evaluator FAIL:',', '.join(failed));sys.exit(1)
print('Stage 14B evaluator PASS: 14B.1-14B.29, 14B.T')
