from pathlib import Path
import argparse, re, sys
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,default=Path(__file__).parent/'evidence'/'serial.txt');p.add_argument('--deliberate-failure',action='store_true');a=p.parse_args()
text=a.evidence.read_text(errors='replace') if a.evidence.exists() else ''
ids=['12.1','12.2','12.2a','12.2b','12.3','12.3a','12.4','12.4a','12.5','12.5a','12.6','12.7','12.8','12.9','12.10','12.11','12.12','12.13','12.14','12.15','12.16','12.17','12.18','12.19','12.20','12.21','12.21a','12.22','12.T']
failed=[]
for i in ids:
    rows=re.findall(rf'ACC S12 test={re.escape(i)}[^\r\n]*',text)
    if not rows or not any(('pass=1' in x) or (i=='12.T' and 'wrong=FAIL restored=PASS' in x) for x in rows): failed.append(i)
if 'pass=0' in text or 'ACC DONE' not in text: failed.append('campaign')
if a.deliberate_failure: failed.append('injected-evaluator-failure')
if failed:
    print('Stage 12 evaluator FAIL:',', '.join(failed));sys.exit(1)
print('Stage 12 evaluator PASS: 12.1-12.22, subtests, and 12.T')
