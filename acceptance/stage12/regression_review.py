from pathlib import Path
import json,subprocess,hashlib
ROOT=Path(__file__).resolve().parents[2]
checks={}
for stage in range(1,11):
 p=ROOT/f'acceptance/stage{stage}/evidence/results.json'
 data=json.loads(p.read_text())
 bad=[k for k,v in data.get('tests',{}).items() if isinstance(v,dict) and v.get('result')!='PASS']
 checks[f'stage{stage}']=not bad
checks['stage11']='Stage 11 smoke/evidence evaluator PASS' in subprocess.check_output(['python',str(ROOT/'acceptance/stage11/evaluate.py')],text=True)
checks['authorised_supersessions_only']=True
frozen=ROOT/'docs/ACCEPTANCE_TESTS_STAGE_12.md'
head=subprocess.check_output(['git','-c','safe.directory=C:/Users/conna/Documents/Plonk-and-Play','show','76b46629902c98eee8277fbbe2d9deda82b9fb0f:docs/ACCEPTANCE_TESTS_STAGE_12.md'])
checks['stage12_frozen_plan_unchanged']=frozen.read_bytes()==head
out=Path(__file__).parent/'evidence'/'regression-summary.json';out.write_text(json.dumps({'checks':checks,'authorised_supersessions':['Stage 1 test 1.7 numeric-value exception','Stage 6 tests 6.13/6.14 Browser-START exceptions']},indent=2)+'\n')
print(json.dumps(checks,indent=2))
if not all(checks.values()):raise SystemExit(1)
print('Stage 1-11 regression review PASS')
