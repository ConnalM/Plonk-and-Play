"""External acceptance oracle. Never changes firmware state or frozen criteria."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
OUT = HERE / 'evidence'
SPEC_COMMIT = 'ac6c73bce1fbfad374eee7409b974663647ce65a'

def git(*args):
    return subprocess.check_output(['git', '-c', f'safe.directory={ROOT.as_posix()}', *args], cwd=ROOT).decode('utf-8')

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def save(name, data):
    (OUT / name).write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')

RECIPES = {
 '1.1': 'Fresh Wokwi ESP32 boot; observe production serial and ready/fault from instrumented composition.',
 '1.2': 'Three shared-service samples, scenario delays of 1 simulated second. Adjacent differences must be 1,000,000 +/-25,000 us (UART/observer scheduling allowance, not a physical-clock accuracy claim).',
 '1.3': 'Fresh simulated flash, observe RAM defaults: lanes=2,laps=10,features=0,sound=1,power=1,mode=1; load status DefaultsMissing=1.',
 '1.4': 'Save laps=37 through real Memory in pp-stage1, ESP.restart, then observe normal lifecycle-loaded RAM and Remembered=0.',
 '1.5': 'Overwrite both pp-stage1 slots with 32 bytes of 0xa5 at Store fault-injection seam; ESP.restart; expect defaults, DefaultsInvalid=2 and ready=1.',
 '1.6': 'After startup, read RAM lap count 10,000 times; sum=100000 and persistent read count unchanged; repeated idle snapshots also must not add reads.',
 '1.7': 'Diagnostics publishes token=271828, relevantTime=1234567890123, eventId=73 on actual running bus. Both authorised subscribers receive the Diagnostics-only probe type/source and exactly one identical payload.',
 '1.8': 'Input endpoint attempts the same Diagnostics-only publication; expect Forbidden=1 and neither authorised subscriber receives it.',
 '1.9': 'Same publication with an attached Diagnostics participant that has not subscribed; expect no receipt for it, while subscribed participants receive.',
 '1.10': 'Run boot self-test and repeat using serial t; retain individual actual check results, raw bus observations, and exact self-test source.',
 '1.11': 'Production image boot with diagnostics enabled; require all four service readiness messages and individual bus checks plus final self-test result.',
 '1.12': 'Same acceptance ELF, set observer-owned next-boot diagnostic switch, ESP.restart with diagnostics disabled before startup. Independent observer must see ready=1/fault=0/diag=0, working config and functioning bus; no product startup diagnostic lines after reset.',
 '1.13': 'Inspect frozen governing documents and exact source; build production and instrumented firmware; link System Time from a second translation unit. Retain structural findings and source snapshot.',
 '1.T': 'Feed the same configuration oracle known-wrong laps=999 against captured first-boot laps=10. Require FAIL and mismatch retained, then restore expected laps=10 and require PASS.'
}

def prepare():
    OUT.mkdir(exist_ok=True)
    spec = (ROOT / 'docs/ACCEPTANCE_TESTS_STAGE_1.md').read_text(encoding='utf-8')
    blocks = re.split(r'^### (1\.(?:\d+|T)) ', spec, flags=re.M)
    tests = {blocks[i]: blocks[i+1].split('\n## ')[0].strip() for i in range(1, len(blocks), 2)}
    save('plan.json', {'project': 'Plonk & Play', 'repository': 'ConnalM/Plonk-and-Play',
        'source_base_commit': git('rev-parse', 'HEAD').strip(), 'acceptance_spec_commit': SPEC_COMMIT,
        'prepared_utc': datetime.now(timezone.utc).isoformat(),
        'frozen_spec_sha256': digest(ROOT / 'docs/ACCEPTANCE_TESTS_STAGE_1.md'),
        'tests': {key: {'frozen_criterion': tests[key], 'predetermined_recipe': recipe} for key, recipe in RECIPES.items()}})
    # Snapshot every build input and harness file; later changes cannot relabel a run.
    entries = {}
    for top in [ROOT / 'firmware', HERE]:
        for path in top.rglob('*'):
            if not path.is_file() or '.pio' in path.parts or 'evidence' in path.parts:
                continue
            rel = path.relative_to(ROOT)
            dest = OUT / 'source' / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, dest)
            entries[rel.as_posix()] = digest(path)
    for path in (ROOT/'docs').glob('*.md'):
        rel=path.relative_to(ROOT)
        entries[rel.as_posix()]=digest(path)
        # The frozen test itself is retained verbatim. Other governing documents
        # are identified by this exact source commit and manifest hash, avoiding
        # redundant copies of the repository's accepted documentation.
        if path.name == 'ACCEPTANCE_TESTS_STAGE_1.md':
            dest=OUT/'source'/rel;dest.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(path,dest)
    save('source-manifest.json', entries)
    (OUT/'working-tree.patch').write_text(git('diff', '--', 'firmware'), encoding='utf-8')

def lines(text, prefix):
    return [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in text.splitlines() if line.startswith(prefix)]

def number(row, key):
    return int(row[key])

def config_oracle(actual, laps=10, status=1):
    expected = dict(status=status,lanes=2,laps=laps,features=0,sound=1,power=1,mode=1)
    observed = {key: number(actual,key) for key in expected}
    return {'expected': expected, 'actual': observed, 'result': 'PASS' if expected==observed else 'FAIL'}

def evaluate():
    plan=json.loads((OUT/'plan.json').read_text(encoding='utf-8'))
    raw=(OUT/'acceptance-serial.txt').read_text(encoding='utf-8')
    prod=(OUT/'production-serial.txt').read_text(encoding='utf-8')
    snapshots=lines(raw,'ACC SNAP ');configs=lines(raw,'ACC CONFIG ')
    snap=lambda phase: [r for r in snapshots if number(r,'phase')==phase]
    config=lambda phase: next(r for r in configs if number(r,'phase')==phase)
    bus=lines(raw,'ACC BUS ');received=lines(raw,'ACC RECEIVED ');auth=lines(raw,'ACC AUTH ')
    artifacts={}
    for env in ['esp32dev','acceptance']:
        for filename in ['firmware.elf','firmware.merged.bin']:
            path=ROOT/'firmware/.pio/build'/env/filename
            dest=OUT/'artifacts'/env/filename;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,dest)
            artifacts[f'{env}/{filename}']={'sha256':digest(path),'bytes':path.stat().st_size}
    save('build-manifest.json', artifacts)
    results={}
    def check(key, ok, actual, evidence='acceptance-serial.txt'):
        results[key]={**plan['tests'][key], 'result':'PASS' if ok else 'FAIL', 'actual':actual,
          'raw_evidence':evidence,'source_manifest':'source-manifest.json','build_manifest':'build-manifest.json'}
    check('1.1', 'STAGE1_PASS' in prod and number(snap(0)[0],'ready')==1 and number(snap(0)[0],'fault')==0,
          {'snapshot':snap(0)[0],'no_race_message':'no race behaviour' in prod}, 'production-serial.txt; acceptance-serial.txt; structural-review.md')
    times=[number(s,'time') for s in snap(0)];deltas=[b-a for a,b in zip(times,times[1:])]
    check('1.2',len(times)==3 and all(975000<=d<=1025000 for d in deltas),{'system_us':times,'delta_us':deltas},'acceptance-serial.txt; source/firmware/src/system_time.cpp; source/firmware/src/acceptance_clock.cpp')
    for key,phase,laps,status in [('1.3',0,10,1),('1.4',4,37,0),('1.5',5,10,2)]:
        result=config_oracle(config(phase),laps,status)
        fixture_ok=phase==0 or f'ACC FIXTURE phase={phase} ok=1' in raw
        check(key,result['result']=='PASS' and number(snap(phase)[0],'ready')==1 and fixture_ok,result)
    ram=lines(raw,'ACC RAM ')[0]
    check('1.6',number(ram,'sum')==100000 and number(ram,'readsBefore')==number(ram,'readsAfter') and len({s['reads'] for s in snap(0)})==1,{'ram_reads':ram,'idle_read_counts':[s['reads'] for s in snap(0)]})
    # This fixture deliberately uses the Diagnostics-only probe, whose
    # accepted numeric value is 7; it is not an INPUT_EVENT (type 2).
    b=bus[0];expected={'type':'7','source':b['publisher'],'token':'271828','time':'1234567890123','id':'73'}
    correct_receipts=all(all(r[k]==v for k,v in expected.items()) for r in received[:2]) and len(received[:2])==2
    check('1.7',correct_receipts and all(b[k]==v for k,v in {'result':'0','a':'1','b':'1','extraA':'0','extraB':'0'}.items()),{'delivery':b,'received':received[:2],'expected_payload':expected})
    check('1.8',auth[0]==dict(result='1',receivedA='0',receivedB='0'),auth[0])
    check('1.9',b['unrelated']=='0' and b['a']=='1' and b['b']=='1',b)
    names=['bus.authority.publish','bus.authority.subscribe','bus.fanout.publish','bus.fanout.identical','bus.no.duplicate','bus.backpressure.atomic','bus.fifo','bus.recovery']
    counts={n:raw.count('[TEST] PASS '+n) for n in names}
    check('1.10',all(c>=2 for c in counts.values()) and '[TEST] FAIL' not in raw,counts,'acceptance-serial.txt; source/firmware/include/pp/verification.h')
    required=['System Time READY','Memory READY','Working configuration READY','Message Bus READY','[BUS SELF-TEST] PASS']
    check('1.11',all(s in prod for s in required),{s:s in prod for s in required},'production-serial.txt')
    quiet=raw.split('ACC FIXTURE phase=12 ok=1',1)[1]
    quiet_snap=snap(12)[0];quiet_bus=next(b for b in bus if b['phase']=='12')
    forbidden=bool(re.search(r'^\[(DEV|INIT|TEST)\]|^STAGE1_PASS|^\[BUS SELF-TEST\]',quiet,re.M))
    check('1.12',quiet_snap['diag']=='0' and quiet_snap['ready']=='1' and quiet_snap['fault']=='0' and quiet_bus['a']=='1' and quiet_bus['b']=='1' and not forbidden,
          {'snapshot':quiet_snap,'bus':quiet_bus,'product_diagnostics_after_reset':forbidden,'same_ELF_for_all_phases':artifacts['acceptance/firmware.elf']['sha256']})
    review=json.loads((OUT/'structural-review.json').read_text())
    check('1.13',all(f['result']=='PASS' for f in review['findings']),review,'structural-review.md; structural-review.json; source/; esp32dev-build.txt; acceptance-build.txt')
    wrong=config_oracle(config(0),laps=999);restored=config_oracle(config(0))
    save('deliberate-failure.json',{'test':'1.T','source_base_commit':plan['source_base_commit'],'acceptance_spec_commit':SPEC_COMMIT,'build_manifest':artifacts,'raw_evidence':'acceptance-serial.txt','wrong_expectation_run':wrong,'restored_expectation_run':restored})
    check('1.T',wrong['result']=='FAIL' and restored['result']=='PASS',{'known_wrong_expected_value':wrong,'restored':restored},'deliberate-failure.json; acceptance-serial.txt')
    save('results.json',{'project':plan['project'],'repository':plan['repository'],'source_base_commit':plan['source_base_commit'],'acceptance_spec_commit':SPEC_COMMIT,'source_is_uncommitted_overlay':True,'builds':artifacts,'tests':results})
    for key,result in results.items():print(key,result['result'])
    return 0 if all(r['result']=='PASS' for r in results.values()) else 1

if __name__=='__main__':
    if sys.argv[1]=='prepare':prepare()
    elif sys.argv[1]=='evaluate':sys.exit(evaluate())
