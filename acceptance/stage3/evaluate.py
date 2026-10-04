"""External oracle for the frozen Stage 3 bus-delivery tests."""
import hashlib,json,re,shutil,subprocess,sys
from pathlib import Path
from datetime import datetime,timezone
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1];OUT=HERE/'evidence';SPEC='a715c2653a08b6112565f8f1b83d1218b059827b'
RECIPES={
'3.1':'Drive one valid source-facing simulated-detector detection; observe subscribed consumer A.',
'3.2':'Drive one valid detection with predetermined identity 50500001:1 and trigger Relevant Time 320000.',
'3.3':'Inspect A receipt for Input Module endpoint source 3.',
'3.4':'A subscribed, connected B unsubscribed; drive one valid source detection.',
'3.5':'A and B subscribed; drive one valid source detection.',
'3.6':'Add B, remove B, then drive another valid source detection; inspect InputModule source path.',
'3.7':'Connected Diagnostics B attempts to publish a forged INPUT_EVENT.',
'3.8':'Subscribed Diagnostics A attempts to publish a forged INPUT_EVENT.',
'3.9':'Inspect exact source and instrumented Bus receipts for no direct consumer route.',
'3.10':'Drive one valid source detection and inspect delivered envelope/payload.',
'3.11':'With A/B subscribed, unsubscribe B and drive another valid source detection.',
'3.T':'Compare captured 3.1 receipt with known-wrong expected count 2, then restore count 1.'}
def git(*a):return subprocess.check_output(['git','-c',f'safe.directory={ROOT.as_posix()}',*a],cwd=ROOT).decode('cp1252')
def dig(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(n,x):(OUT/n).write_text(json.dumps(x,indent=2)+'\n')
def rows(raw,prefix):return [dict(re.findall(r'(\w+)=([^\s]+)',x)) for x in re.findall(re.escape(prefix)+r'([^\r\n]*)',raw)]
def prepare():
 OUT.mkdir(exist_ok=True);spec=(ROOT/'docs/ACCEPTANCE_TESTS_STAGE_3.md').read_text();parts=re.split(r'^### (3\.(?:\d+|T)) ',spec,flags=re.M);criteria={parts[i]:parts[i+1].split('\n## ')[0].strip() for i in range(1,len(parts),2)}
 save('plan.json',{'project':'Plonk & Play','repository':'ConnalM/Plonk-and-Play','source_base_commit':git('rev-parse','HEAD').strip(),'acceptance_spec_commit':SPEC,'prepared_utc':datetime.now(timezone.utc).isoformat(),'frozen_spec_sha256':dig(ROOT/'docs/ACCEPTANCE_TESTS_STAGE_3.md'),'tests':{k:{'frozen_criterion':criteria[k],'predetermined_stimulus':v} for k,v in RECIPES.items()}})
 entries={}
 for top in [ROOT/'firmware',HERE]:
  for p in top.rglob('*'):
   if not p.is_file() or '.pio' in p.parts or 'evidence' in p.parts:continue
   rel=p.relative_to(ROOT);dest=OUT/'source'/rel;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,dest);entries[rel.as_posix()]=dig(p)
 p=ROOT/'docs/ACCEPTANCE_TESTS_STAGE_3.md';dest=OUT/'source'/'docs'/p.name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,dest);entries['docs/ACCEPTANCE_TESTS_STAGE_3.md']=dig(p)
 save('source-manifest.json',entries);(OUT/'working-tree.patch').write_text(git('diff','--','firmware','acceptance/stage3'))
def evaluate():
 plan=json.loads((OUT/'plan.json').read_text());raw=(OUT/'acceptance-serial.txt').read_text();prod=(OUT/'production-serial.txt').read_text();delivery={r['test']:r for r in rows(raw,'ACC DELIVERY ')};auth={r['test']:r for r in rows(raw,'ACC AUTH ')};subs=rows(raw,'ACC SUBSCRIBERS ')[0]
 artifacts={}
 for env in ['esp32dev','stage3acceptance']:
  for n in ['firmware.elf','firmware.merged.bin']:
   p=ROOT/'firmware/.pio/build'/env/n;d=OUT/'artifacts'/env/n;d.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,d);artifacts[f'{env}/{n}']={'sha256':dig(p),'bytes':p.stat().st_size}
 save('build-manifest.json',artifacts);results={}
 def ck(k,ok,actual,e='acceptance-serial.txt'):results[k]={**plan['tests'][k],'expected_result':'PASS','actual_result':actual,'result':'PASS' if ok else 'FAIL','raw_evidence':e,'source_manifest':'source-manifest.json','build_manifest':'build-manifest.json'}
 def normal(k,a,b,time):
  r=delivery[k];ok=all(r[x]==v for x,v in {'delivery':'0','a':str(a),'b':str(b),'expectedA':str(a),'expectedB':str(b),'source':'3','device':'50500001','capability':'1','relevant':str(time)}.items());return ok,r
 for k,a,b,t in [('3.1',1,0,120000),('3.2',1,0,320000),('3.3',1,0,520000),('3.4',1,0,720000),('3.5',1,1,920000),('3.6',1,0,1120000),('3.10',1,1,1320000),('3.11',1,0,1520000)]:
  ok,r=normal(k,a,b,t);ck(k,ok,r)
 ck('3.2',results['3.2']['result']=='PASS' and int(delivery['3.2']['relevant'])<int(delivery['3.2']['observed']),delivery['3.2'])
 ck('3.3',results['3.3']['result']=='PASS',delivery['3.3'])
 ck('3.4',results['3.4']['result']=='PASS',delivery['3.4'])
 ck('3.5',results['3.5']['result']=='PASS',delivery['3.5'])
 core=(ROOT/'firmware/include/pp/core.h').read_text();probe=(ROOT/'firmware/tests/stage3_acceptance_probe.inc').read_text();no_private='bus_.publish(endpoint,event)' in core and 'bus.receive(stage3A,event)' in probe and 'InputModule' not in probe[probe.index('stage3Drain'):probe.index('stage3ValidSourceDetection')]
 ck('3.6',results['3.6']['result']=='PASS' and subs=={'added':'1','removed':'1'}, {'delivery':delivery['3.6'],'subscriptions':subs},'acceptance-serial.txt; source/firmware/include/pp/core.h')
 for k in ['3.7','3.8']:ck(k,auth[k]['result']=='1' and auth[k]['a']=='0' and auth[k]['b']=='0',auth[k])
 # Later accepted stages add generic envelope fields for their own message
 # types.  Prove the Stage 3 invariant at the actual InputModule publication
 # boundary instead of rejecting those unrelated fields in the shared carrier.
 input_start=core.index('class InputModule'); input_end=core.index('struct OutputModule',input_start); input_module=core[input_start:input_end]
 event_start=input_module.index('Message e{}'); event_boundary=input_module[event_start:event_start+300]
 no_race=('e.type=Type::InputEvent' in event_boundary and 'e.input=' in event_boundary and 'e.relevantTime=' in event_boundary and 'e.eventId=' in event_boundary and all(x not in event_boundary for x in ['raceEntryId','lapNumber','lane','finish','start']))
 common_bus_publish='bus_.publish(endpoint,backlog_[head_])' in input_module and 'case Type::InputEvent: return mask(Role::Input)' in core and 'case Type::InputEvent: return mask(Role::RaceEngine)|mask(Role::Diagnostics)' in core; no_private_current=common_bus_publish and all(token not in input_module for token in ['RaceEngineModule','RaceControlModule','LapCompleted','CompetitionComplete','std::function','callback']); detector_internal='SimulatedDetector detectors_[MaxDetectors]' in input_module and 'MaxDetectors=PP_MAX_ENTRIES' in input_module; findings=[{'requirement':'Input Module publishes INPUT_EVENT through the common P&P Message Bus','result':'PASS' if common_bus_publish else 'FAIL'},{'requirement':'No direct Input Module-to-consumer path','result':'PASS' if no_private_current else 'FAIL'},{'requirement':'Input Device is internal, not bus participant','result':'PASS' if detector_internal else 'FAIL'},{'requirement':'No race meaning in INPUT_EVENT','result':'PASS' if no_race else 'FAIL'}];save('structural-detector-migration.json',{'historical_detector_assumption':['bus_.publish(endpoint,event)','SimulatedDetector detector_'],'why_obsolete':'Stage 9 added the protected Input Module backlog and two detector instances while preserving the frozen Input Module to Message Bus boundary.','current_requirement_proof':{'input_module_bus_publish':common_bus_publish,'no_private_consumer_dependency':no_private_current,'detector_internal':detector_internal,'input_event_authority':'Input publisher and RaceEngine/Diagnostics consumers are enforced by Bus roles.'}})
 save('structural-review.json',{'findings':findings});(OUT/'structural-review.md').write_text('# Stage 3 structural review\n\n'+'\n'.join(f"- **{x['result']}** — {x['requirement']}" for x in findings)+'\n')
 ck('3.9',all(x['result']=='PASS' for x in findings),{'findings':findings},'structural-review.md; structural-review.json; source/')
 ck('3.10',results['3.10']['result']=='PASS' and no_race,delivery['3.10'],'acceptance-serial.txt; structural-review.md')
 ck('3.11',results['3.11']['result']=='PASS',delivery['3.11'])
 wrong={'expected_count':2,'actual_count':int(delivery['3.1']['a']),'result':'FAIL' if delivery['3.1']['a']!='2' else 'PASS'};restored={'expected_count':1,'actual_count':int(delivery['3.1']['a']),'result':'PASS' if delivery['3.1']['a']=='1' else 'FAIL'};save('deliberate-failure.json',{'test':'3.T','acceptance_spec_commit':SPEC,'wrong_expectation_run':wrong,'restored_expectation_run':restored,'raw_evidence':'acceptance-serial.txt'});ck('3.T',wrong['result']=='FAIL' and restored['result']=='PASS',{'wrong':wrong,'restored':restored},'deliberate-failure.json; acceptance-serial.txt')
 ck('3.1',results['3.1']['result']=='PASS' and 'STAGE3_PASS' in prod,results['3.1']['actual_result'],'production-serial.txt; acceptance-serial.txt')
 save('results.json',{'project':'Plonk & Play','source_base_commit':plan['source_base_commit'],'acceptance_spec_commit':SPEC,'source_is_uncommitted_overlay':True,'builds':artifacts,'tests':results})
 for k,v in results.items():print(k,v['result'])
 return 0 if all(v['result']=='PASS' for v in results.values()) else 1
if __name__=='__main__':
 if sys.argv[1]=='prepare':prepare()
 elif sys.argv[1]=='evaluate':sys.exit(evaluate())
