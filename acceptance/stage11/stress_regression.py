"""Deterministic Stage 11 scenario/stress plan generator.

The generated JSONL is an input ledger for the real HTTP/fixture runner. It records
seed, ordered legal operations, awkward detector times, reconnect points and the
expected lifecycle checkpoints. A failing run must retain this ledger beside the
observed State/Facts/Request Results evidence.
"""
import argparse, json, random
from pathlib import Path

def scenario(seed, cycles=12):
    rng=random.Random(seed); events=[]; lane_laps=[0,0]
    for n in range(cycles):
        method='HONOUR' if n%2==0 else 'GRID'
        events += [{'op':'PAUSE','cycle':n}, {'op':'SETTLE','cycle':n},
                   {'op':method+'_RESTART','cycle':n}]
        # unequal progress and awkward but deterministic detector ordering
        lane=rng.randrange(2); events.append({'op':'DETECTOR_PASS','lane':lane+1,
            'relevantTime':10000000+n*3000000+rng.randrange(-5000,5001)})
        if n%3==1: events.append({'op':'RECONNECT','cycle':n})
        if method=='GRID': lane_laps[lane] += 1
        else: lane_laps[lane] += rng.randrange(0,2)
        events.append({'expect':'RACING','entries':list(lane_laps)})
    return events

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--seed',type=int,default=11011); ap.add_argument('--cycles',type=int,default=12); ap.add_argument('--output',type=Path,default=Path('acceptance/stage11/evidence/stress-scenario.jsonl')); a=ap.parse_args()
    ev=scenario(a.seed,a.cycles); a.output.parent.mkdir(parents=True,exist_ok=True)
    with a.output.open('w',encoding='utf-8') as f:
        f.write(json.dumps({'seed':a.seed,'cycles':a.cycles,'kind':'stage11-scenario-stress'},sort_keys=True)+'\n')
        for i,e in enumerate(ev): f.write(json.dumps({'sequence':i,**e},sort_keys=True)+'\n')
    print(f'seed={a.seed} cycles={a.cycles} events={len(ev)} ledger={a.output}')
if __name__=='__main__': main()
