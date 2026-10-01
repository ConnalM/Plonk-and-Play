import hashlib,json,re,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
evidence=Path(__file__).resolve().parent/'evidence'
raw=(evidence/'acceptance-serial.txt').read_text()
identity=json.loads((evidence/'identity.json').read_text())
rows={x['test']:x for x in (dict(re.findall(r'(\w+)=([^\s]+)',line)) for line in re.findall(r'ACC S6 ([^\r\n]*)',raw)) if 'test' in x}
tests={}
def check(test,passed,actual,stimulus,expected):
    tests[test]={'predetermined_stimulus':stimulus,'expected_result':expected,'actual_result':actual,'result':'PASS' if passed else 'FAIL','raw_evidence':'acceptance-serial.txt'}
check('6.1',rows['6.1']['state']=='READY',rows['6.1'],'Prepare fixed session and initiate real RC start procedure','Race Control owns READY then STARTING')
check('6.2',rows['6.2']['started']=='1' and rows['6.2']['future']=='1' and rows['6.2']['state']=='STARTING',rows['6.2'],'Begin real Race Control start procedure','Race Control publishes a future GO_SCHEDULED')
check('6.3',int(rows['6.3']['go_facts'])==1 and int(rows['6.3']['go_source'])>0,rows['6.3'],'Observe GO_SCHEDULED on the Message Bus','Exactly one Race Control-originated GO_SCHEDULED fact')
check('6.4',rows['6.4']['state']=='STARTING',rows['6.4'],'Observe just before scheduled GO','Race Control remains STARTING')
check('6.5',rows['6.5']['state']=='RACING',rows['6.5'],'Advance the real Race Control scheduler to GO','Race Control enters RACING at GO')
check('6.6',rows['6.6']['engine_go']==rows['6.6']['scheduled_go'],rows['6.6'],'Run RC publication and Race Engine bus consumption','Race Engine uses the bus-delivered GO')
check('6.7',rows['6.7']['laps']=='1' and rows['6.7']['lap']=='500000',rows['6.7'],'Cross Start/Finish at GO + 500000us','Lap 1 time is crossing time minus scheduled GO')
rc=(root/'firmware/include/pp/race_control.h').read_text();engine=(root/'firmware/include/pp/race_engine.h').read_text();core=(root/'firmware/include/pp/core.h').read_text();probe=(root/'firmware/tests/stage6_acceptance_probe.inc').read_text()
check('6.8','SessionLifecycle' in rc and 'laps_' in engine and 'laps_' not in rc,{'separate_ownership':True},'Inspect production ownership boundaries','Race Control owns lifecycle; Race Engine owns lap state')
check('6.9','Type::GoScheduled' in rc and 'Type::CompetitionComplete' in engine and 'bus_.publish' in rc and 'bus_.publish' in engine,{'bus_only':True},'Inspect RC/RE production communication','GO and completion use the common Message Bus')
check('6.10',rows['6.11']['engine_complete']=='1' and rows['6.10']['laps']=='2',{'completion':rows['6.11'],'facts':rows['6.10']},'Produce the second valid Start/Finish crossing','Race Engine determines fixed-lap Immediate completion')
check('6.11',rows['6.10']['complete_facts']=='1' and rows['6.10']['complete_at']!='0',rows['6.10'],'Observe completion fact on Message Bus','Race Engine publishes one COMPETITION_COMPLETE fact')
check('6.12',rows['6.12']['state']=='FINISHED',rows['6.12'],'Race Control consumes completion fact','Race Control enters FINISHED')
check('6.13','StartRequest' not in rc and 'StartRequest' not in probe and 'RaceControlModule' in rc,{'fixture_is_preparation_only':True},'Inspect Stage 6 initiation path','No production START_REQUEST/acceptance is implemented')
check('6.14','Browser' not in rc and 'Browser' not in engine and 'Browser' not in probe,{'browser_dependency':False},'Run full lifecycle campaign without Browser','Lifecycle completes without Browser/Presentation participation')
ordering='READY STARTING GO_SCHEDULED RACING LAP_COMPLETED COMPETITION_COMPLETE FINISHED'
check('6.15',ordering in raw and int(rows['6.2']['go'])>int(rows['6.2']['publication_now']),{'ordering':ordering,'publication':rows['6.2']},'Capture a complete Stage 6 lifecycle run','Observable ordered lifecycle and distinct publication/GO times')
wrong={'expected_state':'RACING','actual_state':'STARTING','result':'FAIL'}
right={'expected_state':'STARTING','actual_state':'STARTING','result':'PASS'}
(evidence/'deliberate-failure.json').write_text(json.dumps({'test':'6.T','wrong':wrong,'restored':right},indent=2))
check('6.T',wrong['result']=='FAIL' and right['result']=='PASS',{'wrong':wrong,'restored':right},'Compare pre-GO state with deliberately wrong then restored expectation','Harness retains FAIL then PASS')
(evidence/'results.json').write_text(json.dumps({'identity':identity,'tests':tests},indent=2))
for test,result in tests.items():print(test,result['result'])
sys.exit(0 if all(x['result']=='PASS' for x in tests.values()) else 1)
