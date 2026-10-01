import json,re,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2];evidence=Path(__file__).resolve().parent/'evidence';raw=(evidence/'serial.txt').read_text()
rows={x['test']:x for x in (dict(re.findall(r'(\w+)=([^\s]+)',line)) for line in re.findall(r'ACC S7 ([^\r\n]*)',raw))}
tests={}
def check(name,ok):tests[name]={'result':'PASS' if ok else 'FAIL','raw_evidence':'serial.txt'}
check('7.1',rows['7.1']['connected']=='1' and rows['7.1']['initial_revision']=='0' and rows['7.1']['initial_state_reads']=='1' and rows['7.1']['synchronised']=='1')
check('7.2',rows['7.2']['lifecycle']=='READY' and rows['7.2']['entry']=='91' and rows['7.2']['laps']=='0')
check('7.3',rows['7.3']['started']=='1' and rows['7.3']['lifecycle']=='STARTING')
check('7.4',rows['7.4']['entry']=='91' and rows['7.4']['laps']=='1' and rows['7.4']['lap']=='500000')
check('7.5',rows['7.5']['authoritative_laps']==rows['7.5']['browser_laps']=='1')
check('7.6',int(rows['7.6']['notice'])>0 and rows['7.6']['notification_only']=='1')
check('7.7',rows['7.7']['notice_has_state']=='0')
check('7.8',rows['7.8']['lifecycle']=='FINISHED' and rows['7.8']['laps']=='2' and rows['7.8r']['refresh_failed']=='1' and rows['7.8r']['retry_converged']=='1' and rows['7.8r']['lifecycle']=='STARTING')
check('7.9',rows['7.9']['fact_failure_keeps_state']=='1' and rows['7.9']['fact_type']==str(4) and rows['7.9']['fact_lap']=='1' and rows['7.9']['state_laps']=='1')
check('7.10',rows['7.10']['missed_fact']=='1' and rows['7.10']['laps']=='2' and rows['7.10']['lap']=='400000')
check('7.11',rows['7.11']['go']==rows['7.11']['browser_go'] and int(rows['7.11']['go'])>0)
check('7.12',rows['7.12']['disconnected']=='1' and rows['7.12']['authoritative_laps']=='2' and rows['7.12']['lifecycle']=='FINISHED')
check('7.13',rows['7.13']['late']=='1' and rows['7.13']['lifecycle']=='FINISHED' and rows['7.13']['entry']=='91' and rows['7.13']['laps']=='2')
check('7.14',rows['7.14']['slow_client']=='1' and rows['7.14']['authoritative_laps']=='2' and rows['7.14']['complete']=='1')
check('7.15',rows['7.15']['browser_requests']=='0')
check('7.16',rows['7.16']['cache_only']=='1' and rows['7.16']['state_from_noticeboard']=='1')
check('7.T',rows['7.T']['result']=='FAIL' and rows['7.T']['result2']=='PASS')
(evidence/'deliberate-failure.json').write_text(json.dumps({'test':'7.T','wrong_expectation':'one lap','actual':'two laps','result':'FAIL','restored':'PASS'},indent=2))
(evidence/'results.json').write_text(json.dumps({'acceptance_spec_commit':'4b6722a663bd1365c49a7dd279dcc721d3d14806','tests':tests},indent=2))
for name,test in tests.items():print(name,test['result'])
sys.exit(0 if all(test['result']=='PASS' for test in tests.values()) else 1)
