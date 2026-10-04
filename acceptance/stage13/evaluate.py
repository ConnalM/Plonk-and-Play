from pathlib import Path
import argparse, re, sys
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,default=Path(__file__).parent/'evidence'/'serial.txt');p.add_argument('--deliberate-failure',action='store_true');a=p.parse_args()
text=a.evidence.read_text(errors='replace') if a.evidence.exists() else ''
ids=['13.1','13.2','13.3','13.4','13.5','13.6','13.7','13.8','13.9','13.10','13.11','13.12','13.13','13.14','13.15','13.16','13.17','13.18','13.19','13.20','13.21','13.22','13.23','13.24','13.25','13.T']
failed=[]
for i in ids:
    rows=re.findall(rf'ACC S13 test={re.escape(i)}[^\r\n]*',text)
    if not rows or not any(('pass=1' in x) or (i=='13.T' and 'wrong=FAIL restored=PASS' in x) for x in rows): failed.append(i)
if 'pass=0' in text or 'ACC DONE' not in text: failed.append('campaign')
if a.deliberate_failure: failed.append('injected-evaluator-failure')
if failed:
    print('Stage 13 evaluator FAIL:',', '.join(failed));sys.exit(1)
print('Stage 13 evaluator PASS: 13.1-13.25 and 13.T')


