"""External Stage 2 acceptance oracle; it never changes firmware state or criteria."""
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
SPEC_COMMIT = 'bc6693061f2ed4ad99b51d7109c20f271c7aa1e0'

RECIPES = {
 '2.1':'Source fixture drives INACTIVE → ACTIVE → INACTIVE with stable samples at 100000/120000/121000/141000 us.',
 '2.2':'Source fixture drives one valid ACTIVE then samples it again while remaining ACTIVE.',
 '2.3':'Source fixture drives two valid active periods separated by a valid clear/re-arm.',
 '2.4':'Source fixture drives a short active chatter period, then a valid active period.',
 '2.5':'After one valid trigger the fixture drives short inactive chatter, then a valid clear and second valid trigger.',
 '2.6':'Source fixture recognises at 820000 us and leaves the bus event queued until observation at 900000 us.',
 '2.7':'Fixture emits a valid event, restarts the ESP32, then emits another valid event from the same declared capability.',
 '2.8':'Inspect captured standard physical event fields and exact InputEvent source/struct definition.',
 '2.9':'Review fixture and source: fixture calls InputModule.sampleSource, which calls its contained detector before publishing.',
 '2.10':'Review source/build: detector owns source-specific state; Input Module publishes; no race terms appear in this boundary.',
 '2.T':'Evaluate captured 2.1 observation against a known-wrong count of 2, retain FAIL, then restore expected count 1.'
}

def git(*args):
    return subprocess.check_output(['git','-c',f'safe.directory={ROOT.as_posix()}',*args],cwd=ROOT).decode()
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def save(name,data): (OUT/name).write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
def fields(text,prefix):
    # Wokwi may deliver a scenario write immediately after matching a serial
    # substring, before the UART's trailing newline has arrived. Locate our
    # explicit record marker rather than treating terminal line framing as data.
    return [dict(re.findall(r'(\w+)=([^\s]+)',line))
            for line in re.findall(re.escape(prefix)+r'([^\r\n]*)',text)]

def prepare():
    OUT.mkdir(exist_ok=True)
    spec=(ROOT/'docs/ACCEPTANCE_TESTS_STAGE_2.md').read_text(encoding='utf-8')
    blocks=re.split(r'^### (2\.(?:\d+|T)) ',spec,flags=re.M)
    criteria={blocks[i]:blocks[i+1].split('\n## ')[0].strip() for i in range(1,len(blocks),2)}
    save('plan.json',{'project':'Plonk & Play','repository':'ConnalM/Plonk-and-Play',
      'source_base_commit':git('rev-parse','HEAD').strip(),'acceptance_spec_commit':SPEC_COMMIT,
      'prepared_utc':datetime.now(timezone.utc).isoformat(),'frozen_spec_sha256':digest(ROOT/'docs/ACCEPTANCE_TESTS_STAGE_2.md'),
      'tests':{key:{'frozen_criterion':criteria[key],'predetermined_stimulus':recipe} for key,recipe in RECIPES.items()}})
    entries={}
    for top in [ROOT/'firmware',HERE]:
      for path in top.rglob('*'):
        if not path.is_file() or '.pio' in path.parts or 'evidence' in path.parts: continue
        rel=path.relative_to(ROOT); dest=OUT/'source'/rel; dest.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(path,dest);entries[rel.as_posix()]=digest(path)
    frozen=ROOT/'docs/ACCEPTANCE_TESTS_STAGE_2.md'; dest=OUT/'source'/'docs'/frozen.name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(frozen,dest)
    entries['docs/ACCEPTANCE_TESTS_STAGE_2.md']=digest(frozen)
    save('source-manifest.json',entries)
    (OUT/'working-tree.patch').write_text(git('diff','--','firmware','acceptance/stage2'),encoding='utf-8')

def evaluate():
    plan=json.loads((OUT/'plan.json').read_text())
    raw=(OUT/'acceptance-serial.txt').read_text(encoding='utf-8')
    production=(OUT/'production-serial.txt').read_text(encoding='utf-8')
    events={row['test']:row for row in fields(raw,'ACC INPUT ')}
    devices=fields(raw,'ACC DEVICE ')
    artifacts={}
    for env in ['esp32dev','stage2acceptance']:
      for name in ['firmware.elf','firmware.merged.bin']:
        path=ROOT/'firmware/.pio/build'/env/name; dest=OUT/'artifacts'/env/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,dest)
        artifacts[f'{env}/{name}']={'sha256':digest(path),'bytes':path.stat().st_size}
    save('build-manifest.json',artifacts)
    results={}
    def check(key,ok,actual,evidence='acceptance-serial.txt'):
      results[key]={**plan['tests'][key],'expected_result':'PASS','actual_result':actual,'result':'PASS' if ok else 'FAIL',
        'raw_evidence':evidence,'source_manifest':'source-manifest.json','build_manifest':'build-manifest.json'}
    expected_counts={'2.1':1,'2.2':1,'2.3':2,'2.4':1,'2.5':2}
    for key,count in expected_counts.items():
      row=events[key]; ok=int(row['count'])==count and int(row['expected'])==count and row['device']=='50500001' and row['capability']=='1'
      check(key,ok,row)
    row=events['2.6']; check('2.6',int(row['count'])==1 and row['relevant']=='820000' and row['observed']=='900000' and int(row['relevant'])<int(row['observed']),row)
    identities=[(d['device'],d['capability']) for d in devices]
    check('2.7',identities==[('50500001','1'),('50500001','1')] and events['2.7a']['count']=='1' and events['2.7b']['count']=='1',{'devices':devices,'events':[events['2.7a'],events['2.7b']]})
    core=(ROOT/'firmware/include/pp/core.h').read_text(); probe=(ROOT/'firmware/tests/stage2_acceptance_probe.inc').read_text()
    # The Bus envelope has legitimately gained fields for later message types.
    # Stage 2's frozen boundary is that the Input Module's INPUT_EVENT
    # publication uses only physical input identity/time/event identity and
    # does not populate or derive race meaning from that shared envelope.
    input_module=re.search(r'class InputModule \{(.*?)(?=\n\};)',core,re.S).group(1)
    forbidden=['raceentry','lane','startfinish','sector','drag','speed','mug']
    no_race=not any(word in input_module.lower() for word in forbidden)
    check('2.8',all(set(row).issuperset({'test','count','device','capability','relevant'}) for row in events.values()) and no_race,
      {'captured_fields':['test','count','device','capability','relevant','observed'],'forbidden_input_interpretation_present':not no_race},'acceptance-serial.txt; source/firmware/include/pp/core.h')
    source_path=('input.sampleSource(active,at)' in probe and 'class InputModule' in core and 'detectors_[which].sample(active,at,trigger)' in core and 'bus_.publish(endpoint,backlog_[head_])' in core)
    check('2.9',source_path,{'fixture_calls':'InputModule.sampleSource','module_path':'InputDevice sample then InputModule publication'},'source/firmware/tests/stage2_acceptance_probe.inc; source/firmware/include/pp/core.h')
    findings=[
      {'requirement':'Input Device owns source conditioning/re-arm','result':'PASS' if all(x in core for x in ['class SimulatedDetector','sourceActive_','armed_','DetectionStableUs','ClearStableUs']) else 'FAIL'},
      {'requirement':'Input Device is contained by Input Module','result':'PASS' if 'SimulatedDetector detectors_[2]' in core and 'Role::Input' not in core[core.index('class SimulatedDetector'):core.index('class InputModule')] else 'FAIL'},
      {'requirement':'Input Module publishes INPUT_EVENT','result':'PASS' if 'e.type=Type::InputEvent' in core and 'bus_.publish(endpoint,backlog_[head_])' in core else 'FAIL'},
      {'requirement':'No central translator or race interpretation','result':'PASS' if no_race else 'FAIL'}]
    save('structural-review.json',{'findings':findings})
    (OUT/'structural-review.md').write_text('# Stage 2 structural review\n\n'+'\n'.join(f"- **{f['result']}** — {f['requirement']}" for f in findings)+'\n',encoding='utf-8')
    check('2.10',all(f['result']=='PASS' for f in findings),{'findings':findings},'structural-review.md; structural-review.json; source/firmware/include/pp/core.h')
    wrong={'expected_count':2,'actual_count':int(events['2.1']['count']),'result':'FAIL' if int(events['2.1']['count'])!=2 else 'PASS'}
    restored={'expected_count':1,'actual_count':int(events['2.1']['count']),'result':'PASS' if int(events['2.1']['count'])==1 else 'FAIL'}
    save('deliberate-failure.json',{'test':'2.T','acceptance_spec_commit':SPEC_COMMIT,'raw_evidence':'acceptance-serial.txt','wrong_expectation_run':wrong,'restored_expectation_run':restored})
    check('2.T',wrong['result']=='FAIL' and restored['result']=='PASS',{'wrong_expectation_run':wrong,'restored_expectation_run':restored},'deliberate-failure.json; acceptance-serial.txt')
    check('2.1',results['2.1']['result']=='PASS' and 'STAGE2_PASS' in production,results['2.1']['actual_result'],'production-serial.txt; acceptance-serial.txt')
    save('results.json',{'project':'Plonk & Play','source_base_commit':plan['source_base_commit'],'acceptance_spec_commit':SPEC_COMMIT,
      'source_is_uncommitted_overlay':True,'builds':artifacts,'tests':results})
    for key,result in results.items(): print(key,result['result'])
    return 0 if all(result['result']=='PASS' for result in results.values()) else 1

if __name__=='__main__':
    if sys.argv[1]=='prepare': prepare()
    elif sys.argv[1]=='evaluate': sys.exit(evaluate())
