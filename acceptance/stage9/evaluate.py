import json,re,sys,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[2]; ev=Path(__file__).parent/'evidence'; raw_file=ev/'serial-current.txt'; raw=(raw_file if raw_file.exists() else ev/'serial-retry.txt').read_text()
rows={m.group(1):m.group(2) for m in re.finditer(r'ACC S9 test=([\d.T]+) (.*)',raw)}
tests={}
for i in range(1,21):
 k=f'9.{i}'; ok='result=PASS' in rows.get(k,''); tests[k]={'result':'PASS' if ok else 'FAIL','actual':rows.get(k,''),'raw_evidence':raw_file.name}
tests['9.T']={'result':'PASS' if 'wrong=FAIL restored=PASS' in rows.get('9.T','') else 'FAIL','actual':rows.get('9.T',''),'raw_evidence':raw_file.name}
human=ev/'human-browser-checkpoint.md'
human_pass=human.exists() and 'Final Browser State was synchronised' in human.read_text()
(ev/'results.json').write_text(json.dumps({'frozen_plan_commit':'09c03b45f8ecd9dce9f903f169cf52f0bf931ccb','tests':tests,'human_browser_checkpoint':{'result':'PASS' if human_pass else 'FAIL','evidence':human.name}},indent=2)+'\n')
print('\n'.join(f'{k} {v["result"]}' for k,v in tests.items()));sys.exit(0 if human_pass and all(x['result']=='PASS' for x in tests.values()) else 1)
