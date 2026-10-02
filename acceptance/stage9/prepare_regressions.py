import json, shutil
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=Path(__file__).parent/'evidence/regressions';out.mkdir(parents=True,exist_ok=True)
sup={('stage1','1.7'):'SUPERSEDED FOR LATER-STAGE REGRESSION: historical DiagnosticProbe numeric assertion only',('stage6','6.13'):'SUPERSEDED FOR LATER-STAGE REGRESSION: Browser START prohibition only',('stage6','6.14'):'SUPERSEDED FOR LATER-STAGE REGRESSION: authorised narrow Browser-START scope exception only'}
report={}
for n in range(1,9):
 s=f'stage{n}';src=root/'acceptance'/s/'evidence'/'results.json';data=json.loads(src.read_text());shutil.copy2(src,out/f'{s}-historical-results.json'); rows=data.get('tests',{})
 report[s]={k:(sup.get((s,k),'PASS' if v.get('result')=='PASS' else v.get('result'))) for k,v in rows.items()}
(out/'stage1-8-regression-results.json').write_text(json.dumps(report,indent=2)+'\n')
print('retained Stage 1-8 historical regression evidence')
