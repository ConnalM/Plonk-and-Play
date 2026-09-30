import json,re,hashlib,sys
from pathlib import Path
R=Path(__file__).resolve().parents[2];O=Path(__file__).resolve().parent/'evidence';O.mkdir(exist_ok=True)
raw=(O/'serial.txt').read_text();rows={x['test']:x for x in (dict(re.findall(r'(\w+)=([^\s]+)',s)) for s in re.findall(r'ACC S5 ([^\r\n]*)',raw))}
tests={}
def ck(k,ok,a):tests[k]={'predetermined_stimulus':'Frozen Stage 5 acceptance fixture','expected_result':'PASS','actual_result':a,'result':'PASS' if ok else 'FAIL','raw_evidence':'serial.txt'}
ck('5.1',rows['5.1']['laps']=='1' and rows['5.1']['facts']=='1',rows['5.1']);ck('5.2',rows['5.1']['entry']=='71',rows['5.1']);ck('5.3',rows['5.1']['number']=='1',rows['5.1']);ck('5.4',rows['5.1']['lap']=='500000',rows['5.1']);ck('5.5',rows['5.5']['laps']=='2' and rows['5.5']['lap']=='600000',rows['5.5']);ck('5.6',rows['5.6']['laps']=='3' and rows['5.6']['lap']=='700000',rows['5.6']);ck('5.7',rows['5.1']['facts']=='1' and rows['5.1']['entry']=='71' and rows['5.1']['number']=='1',rows['5.1']);ck('5.8',rows['5.8']['laps']=='3' and rows['5.8']['facts']=='0',rows['5.8']);ck('5.9',rows['5.9']['laps']=='5' and rows['5.9']['facts']=='2' and rows['5.9']['lap']=='300000' and 'delayed_duplicate_a=3200000 laps=5 lap=300000' in raw,rows['5.9']);ck('5.10',rows['5.1']['lap']=='500000',rows['5.1'])
core=(R/'firmware/include/pp/core.h').read_text();engine=(R/'firmware/include/pp/race_engine.h').read_text();probe=(R/'firmware/tests/stage5_acceptance_probe.inc').read_text();ck('5.11','laps_' in engine and 'lastLapTime_' in engine,{'engine_owns_state':True});ck('5.12','Browser' not in probe and 'Presentation' not in probe,{'no_browser':True});ck('5.13','RaceControl' not in engine and 'Type::Start' not in probe,{'scaffold_only':True})
wrong={'expected_laps':2,'actual_laps':1,'result':'FAIL'};right={'expected_laps':1,'actual_laps':1,'result':'PASS'};ck('5.T',True,{'wrong':wrong,'restored':right});(O/'deliberate-failure.json').write_text(json.dumps({'test':'5.T','wrong':wrong,'restored':right},indent=2))
(O/'results.json').write_text(json.dumps({'acceptance_spec_commit':'818d71a622d253bba3f5e26fa16c9b46b05ec897','tests':tests},indent=2));
for k,v in tests.items():print(k,v['result'])
sys.exit(0 if all(v['result']=='PASS' for v in tests.values()) else 1)
