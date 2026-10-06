from pathlib import Path
import re,sys,json
root=Path(__file__).parent;text=(root/'evidence'/'serial.txt').read_text(errors='replace') if (root/'evidence'/'serial.txt').exists() else ''
ids=[*(f'14C.{i}' for i in range(1,29)),'14C.T'];failed=[]
for ident in ids:
 rows=re.findall(rf'ACC S14C test={re.escape(ident)}[^\r\n]*',text)
 if not rows or not any('pass=1' in row for row in rows): failed.append(ident)
if 'pass=0' in text or 'ACC DONE' not in text: failed.append('campaign')
if not re.search(r'ACC S14C regression=finish_current_lap_one_crossing pass=1',text): failed.append('finish_current_lap_regression')
if not re.search(r'ACC S14C regression=finish_current_lap_reversed_delivery pass=1',text): failed.append('finish_current_lap_reversed_delivery')
if not re.search(r'ACC S14C regression=finish_current_lap_no_origin pass=1',text): failed.append('finish_current_lap_no_origin')
try:
 p=json.loads((root/'evidence'/'evaluator-integrity.json').read_text())
 if p.get('corrupted_evaluator')!='FAIL' or p.get('restored_evaluator')!='PASS':failed.append('14C.27')
except Exception:failed.append('14C.27')
if failed:print('Stage 14C evaluator FAIL:',', '.join(failed));sys.exit(1)
print('Stage 14C evaluator PASS: 14C.1-14C.28, 14C.T')
