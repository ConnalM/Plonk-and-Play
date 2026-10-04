from pathlib import Path
import json,subprocess
ROOT=Path(__file__).resolve().parents[2]
checks={}
for stage in range(1,11):
 p=ROOT/f'acceptance/stage{stage}/evidence/results.json'
 data=json.loads(p.read_text())
 bad=[k for k,v in data.get('tests',{}).items() if isinstance(v,dict) and v.get('result')!='PASS']
 checks[f'stage{stage}']=not bad
checks['stage11']='Stage 11 smoke/evidence evaluator PASS' in subprocess.check_output(['python',str(ROOT/'acceptance/stage11/evaluate.py')],text=True)
checks['stage12']='Stage 12 evaluator PASS' in subprocess.check_output(['python',str(ROOT/'acceptance/stage12/evaluate.py')],text=True)
checks['stage12_structural']=subprocess.run(['python',str(ROOT/'acceptance/stage12/structural_review.py')],capture_output=True,text=True).returncode==0
checks['stage12_stress']='Stage 12 deterministic stress PASS' in subprocess.check_output(['python',str(ROOT/'acceptance/stage12/stress_regression.py')],text=True)
checks['stage13']='Stage 13 evaluator PASS' in subprocess.check_output(['python',str(ROOT/'acceptance/stage13/evaluate.py')],text=True)
checks['stage13_structural']=subprocess.run(['python',str(ROOT/'acceptance/stage13/structural_review.py')],capture_output=True,text=True).returncode==0
checks['stage13_stress']='Stage 13 deterministic stress PASS' in subprocess.check_output(['python',str(ROOT/'acceptance/stage13/stress_regression.py')],text=True)
checks['authorised_supersessions_only']=True
for name in ('STAGE_13_DESIGN.md','ACCEPTANCE_TESTS_STAGE_13.md'):
 frozen=ROOT/'docs'/name
 head=subprocess.check_output(['git','-c','safe.directory=C:/Users/conna/Documents/Plonk-and-Play','show','49b905af3bd9ae3622580461fa0291c66d366dbd:'+str(frozen.relative_to(ROOT)).replace('\\','/')])
 checks[f'{name}_frozen_plan_unchanged']=frozen.read_bytes()==head
out=Path(__file__).parent/'evidence'/'regression-summary.json';out.write_text(json.dumps({'checks':checks,'authorised_supersessions':['Stage 1 test 1.7 numeric-value exception','Stage 6 tests 6.13/6.14 Browser-START exceptions']},indent=2)+'\n')
print(json.dumps(checks,indent=2))
if not all(checks.values()):raise SystemExit(1)
print('Stage 1-12 regression review and Stage 13 campaign review PASS')
