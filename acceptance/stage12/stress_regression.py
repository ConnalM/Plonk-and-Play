from pathlib import Path
import argparse,json,random
p=argparse.ArgumentParser();p.add_argument('--seed',type=int,default=12012);p.add_argument('--runs',type=int,default=200);p.add_argument('--output',type=Path,default=Path(__file__).parent/'evidence'/'stress-scenarios.jsonl');a=p.parse_args();rng=random.Random(a.seed);a.output.parent.mkdir(parents=True,exist_ok=True)
with a.output.open('w') as f:
 f.write(json.dumps({'seed':a.seed,'runs':a.runs,'kind':'stage12-result-order-stress'},sort_keys=True)+'\n')
 for run in range(a.runs):
  target=rng.randint(1,12);mode=rng.choice(['IMMEDIATE','CURRENT','FULL']);laps=[0,0];times=[];t=1000000
  while max(laps)<target or (mode=='FULL' and min(laps)<target):
   lane=rng.randrange(2);t+=rng.randint(1,100000);laps[lane]+=1;times.append((t,lane,laps[lane]))
   if mode=='IMMEDIATE' and max(laps)>=target:break
   if mode=='CURRENT' and max(laps)>=target and all(x>=target-1 for x in laps):break
  W=min(x[0] for x in times if x[2]>=target);F=W if mode=='IMMEDIATE' else max(x[0] for x in times if x[0]>=W)
  assert W<=F and all(times[i][0]<times[i+1][0] for i in range(len(times)-1))
  f.write(json.dumps({'run':run,'mode':mode,'target':target,'W':W,'F':F,'laps':laps,'events':times},sort_keys=True)+'\n')
print(f'Stage 12 deterministic stress PASS seed={a.seed} runs={a.runs}')
