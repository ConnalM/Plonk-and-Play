from pathlib import Path
import argparse,json,re,sys
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,default=Path(__file__).parent/'evidence'/'serial.txt');a=p.parse_args()
text=a.evidence.read_text(errors='replace') if a.evidence.exists() else ''
ids=[*(f'14A.{i}' for i in range(1,24)),'14A.T']
failed=[]
for i in ids:
    rows=re.findall(rf'ACC S14A test={re.escape(i)}[^\r\n]*',text)
    if not rows or not any('pass=1' in row for row in rows): failed.append(i)
if 'pass=0' in text or 'ACC DONE' not in text: failed.append('campaign')
integrity_path=Path(__file__).parent/'evidence'/'evaluator-integrity.json'
try:
    integrity=json.loads(integrity_path.read_text(encoding='utf-8'))
    if integrity.get('corrupted_evaluator')!='FAIL' or integrity.get('restored_evaluator')!='PASS': failed.append('14A.T')
except Exception:
    failed.append('14A.T')
if failed:
    print('Stage 14A evaluator FAIL:',', '.join(failed));sys.exit(1)
print('Stage 14A evaluator PASS: 14A.1-14A.23 and 14A.T')
