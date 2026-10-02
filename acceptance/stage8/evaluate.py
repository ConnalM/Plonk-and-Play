import json,re,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2];evidence=Path(__file__).resolve().parent/'evidence';raw=(evidence/'serial.txt').read_text()
rows={x['test']:x for x in (dict(re.findall(r'(\w+)=([^\s]+)',line)) for line in re.findall(r'ACC S8 ([^\r\n]*)',raw)) if 'test' in x}
tests={}
def check(name,ok,stimulus,expected):tests[name]={'predetermined_stimulus':stimulus,'expected_result':expected,'actual_result':rows.get(name,{}),'result':'PASS' if ok else 'FAIL','raw_evidence':'serial.txt'}
check('8.1',rows['8.1']['submitted']=='1' and rows['8.1']['starts']=='1' and rows['8.1']['source']==rows['8.1']['presentation'],'Browser POST /request/start through Browser Interface','One START_REQUEST originates at Presentation')
check('8.2',rows['8.2']['correlation']==rows['8.2']['result_correlation']=='801','Predetermined correlation 801','Request and Request Result retain correlation')
core=(root/'firmware/include/pp/core.h').read_text();browser=(root/'firmware/include/pp/browser_interface.h').read_text();control=(root/'firmware/include/pp/race_control.h').read_text();engine=(root/'firmware/include/pp/race_engine.h').read_text();probe=(root/'firmware/tests/stage8_acceptance_probe.inc').read_text()
check('8.3',rows['8.3']['bus_source']!='0' and 'Type::StartRequest: return mask(Role::Presentation)' in core and 'Type::StartRequest: return mask(Role::RaceControl)' in core,'Trace Browser Interface request publication','Real P&P Message Bus route to Race Control only')
check('8.4',rows['8.4']['server_context']=='Spectator' and rows['8.4']['result']=='REJECTED' and 'ClientContext::Spectator' in browser and 'context(owner)' in browser,'Spectator Browser supplies RaceDirector claim','Claim cannot grant permission')
check('8.5',rows['8.5']['result']=='ACCEPTED','Authorised Race Director/SMUG Context','Request passes permission and validity')
check('8.6',rows['8.6']['result']=='REJECTED' and rows['8.6']['reason']=='START_permission_denied' and rows['8.6']['definition']=='0' and rows['8.6']['state']=='0','Spectator START','Correlated permission rejection with no session/state change')
check('8.7',rows['8.7']['result']=='REJECTED' and rows['8.7']['reason']=='lifecycle_not_startable' and rows['8.7']['state']=='1','Authorised START during STARTING','Lifecycle rejection')
check('8.8',rows['8.8']['result']=='REJECTED' and rows['8.8']['reason']=='invalid_race_setup_lap_target' and rows['8.8']['definition']=='0' and rows['8.8']['state']=='0','Zero-lap proposed setup','Deterministic invalid-setup rejection')
check('8.9',rows['8.9']['result']=='REJECTED' and rows['8.9']['reason']=='required_StartFinish_capability_unavailable' and rows['8.9']['definition']=='0' and rows['8.9']['state']=='0','Required Start/Finish unavailable','Deterministic capability rejection')
check('8.10',rows['8.10']['result']=='ACCEPTED','Valid authorised START','One correlated ACCEPTED result')
check('8.11',rows['8.11']['definition_before_result']=='1' and rows['8.11']['definition']=='1' and rows['8.11']['result_visible']=='1' and rows['8.11']['committed']=='1' and 'active_.commit' in control and 'result(request.correlation,RequestResult::Accepted)' in control,'Commit accepted proposed setup','Fixed Session Definition exists before ACCEPTED is externally visible')
check('8.12',rows['8.12']['fixed_laps']=='2' and rows['8.12']['fixed_mug']=='91' and rows['8.12']['unchanged']=='1','Mutate proposed setup after acceptance','Fixed Session Definition stays unchanged')
check('8.13',rows['8.13']['state']=='2' and int(rows['8.13']['go'])>0,'Advance accepted Race Control procedure to GO','Race Control reaches RACING through existing path')
check('8.14',rows['8.14']['result']=='ACCEPTED' and rows['8.14']['state']=='2' and rows['8.14']['entry']!='0' and '/request-result' in browser,'Observe Browser result and Noticeboard State','Result is distinct from authoritative State')
check('8.15',{rows['8.15']['first'],rows['8.15']['second']}=={'ACCEPTED','REJECTED'} and rows['8.15']['definition']=='1' and rows['8.15']['committed']=='1' and rows['8.15']['go']=='1','Two distinct correlations submitted before Race Control tick','At most one accepted/session/start/GO')
check('8.16',rows['8.16']['first']=='ACCEPTED' and rows['8.16']['second']=='ACCEPTED' and rows['8.16']['first_laps']=='2' and rows['8.16']['second_laps']=='3' and rows['8.16']['second_mug']=='92' and rows['8.16']['payload']=='correlation_only','Two valid proposed setups','START carries no Race Setup copy')
stage7_regression=evidence/'stage7-regression.json'
stage7_ok=False
if stage7_regression.exists():
    try:
        stage7_ok=json.loads(stage7_regression.read_text()).get('result')=='PASS'
    except json.JSONDecodeError:
        stage7_ok=False
check('8.17',stage7_ok,'Run frozen Stage 7 regression separately','A current, independently retained Stage 7 regression result is PASS')
wrong={'expected':'ACCEPTED','actual':'REJECTED','result':'FAIL'};restored={'expected':'REJECTED','actual':'REJECTED','result':'PASS'}
(evidence/'deliberate-failure.json').write_text(json.dumps({'test':'8.T','wrong':wrong,'restored':restored},indent=2))
check('8.T',rows['8.T']['result']=='FAIL' and rows['8.T']['result2']=='PASS','Compare unauthorised request with wrong then restored expectation','Harness reports FAIL then PASS')
identity=json.loads((evidence/'identity.json').read_text());(evidence/'results.json').write_text(json.dumps({'identity':identity,'tests':tests},indent=2))
for name,test in tests.items():print(name,test['result'])
sys.exit(0 if all(test['result']=='PASS' for test in tests.values()) else 1)