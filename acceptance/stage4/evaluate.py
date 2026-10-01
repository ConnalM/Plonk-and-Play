import json,re,hashlib,sys
from pathlib import Path
R=Path(__file__).resolve().parents[2];O=Path(__file__).resolve().parent/'evidence'; SPEC='eb6a1b8a4e7912c1d3b471ede96578c978f0a909'
def save(n,x):(O/n).write_text(json.dumps(x,indent=2)+'\n')
def rows(t):return {x['test']:x for x in (dict(re.findall(r'(\w+)=([^\s]+)',s)) for s in re.findall(r'ACC S4 ([^\r\n]*)',t)) if 'test' in x}
t=(O/'acceptance-serial.txt').read_text();d=rows(t);core=(R/'firmware/include/pp/core.h').read_text();session_header=(R/'firmware/include/pp/session_definition.h').read_text();probe=(R/'firmware/tests/stage4_acceptance_probe.inc').read_text()
build=R/'firmware/.pio/build/stage4acceptance/firmware.elf';save('build-manifest.json',{'stage4acceptance/firmware.elf':{'sha256':hashlib.sha256(build.read_bytes()).hexdigest(),'bytes':build.stat().st_size}})
save('source-manifest.json',{'firmware/include/pp/core.h':hashlib.sha256((R/'firmware/include/pp/core.h').read_bytes()).hexdigest(),'firmware/include/pp/session_definition.h':hashlib.sha256((R/'firmware/include/pp/session_definition.h').read_bytes()).hexdigest(),'firmware/tests/stage4_acceptance_probe.inc':hashlib.sha256((R/'firmware/tests/stage4_acceptance_probe.inc').read_bytes()).hexdigest(),'docs/ACCEPTANCE_TESTS_STAGE_4.md':hashlib.sha256((R/'docs/ACCEPTANCE_TESTS_STAGE_4.md').read_bytes()).hexdigest()})
tests={}
def ck(k,ok,a):tests[k]={'predetermined_stimulus':'Stage 4 source-facing fixture and fixed session data','expected_result':'PASS','actual_result':a,'result':'PASS' if ok else 'FAIL','raw_evidence':'acceptance-serial.txt'}
for k,lane,time in [('4.1',1,'120000'),('4.2',1,'320000'),('4.3',1,'520000'),('4.4',1,'720000'),('4.5',2,'920000')]:
 r=d[k];ck(k,r['events']=='1' and r['device']=='50500001' and r['capability']=='1' and r['lane']==str(lane) and r['role']=='0' and r['relevant']==time,r)
session=session_header[session_header.index('class SessionDefinition'):session_header.index('};',session_header.index('class SessionDefinition'))+2]
ck('4.6','class SessionDefinition' in session and all(x not in session for x in ['GPIO','Browser','threshold']),{'minimal_data':True})
ck('4.7','class SessionDefinition' in session and 'Bus' not in session,{'data_not_bus_participant':True})
ck('4.8','SessionDefinition' not in core[core.index('class SimulatedDetector'):core.index('struct OutputModule')],{'input_path_independent':True})
ck('4.9','Lap' not in probe and 'RaceEngine' not in probe,{'no_race_engine':True})
ck('4.10','Stage 4 test/session-preparation scaffolding only' in probe and 'Type::Start' not in probe and 'RaceControl' not in probe,{'scaffold_only':True})
wrong={'expected_lane':2,'actual_lane':int(d['4.1']['lane']),'result':'FAIL'};restored={'expected_lane':1,'actual_lane':int(d['4.1']['lane']),'result':'PASS'};save('deliberate-failure.json',{'test':'4.T','wrong':wrong,'restored':restored});ck('4.T',wrong['result']=='FAIL' and restored['result']=='PASS',{'wrong':wrong,'restored':restored})
save('results.json',{'project':'Plonk & Play','source_base_commit':'eb6a1b8a4e7912c1d3b471ede96578c978f0a909','acceptance_spec_commit':SPEC,'tests':tests})
for k,v in tests.items():print(k,v['result'])
sys.exit(0 if all(v['result']=='PASS' for v in tests.values()) else 1)
