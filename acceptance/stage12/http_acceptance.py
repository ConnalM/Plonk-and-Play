"""Stage 12 real-HTTP acceptance against the production Browser Interface.
Requires a freshly booted stage12demo image and uses independent cookie stores.
"""
import json,sys,time
from urllib.request import Request,build_opener,HTTPCookieProcessor,ProxyHandler
from urllib.error import HTTPError,URLError
from http.cookiejar import CookieJar
BASE=sys.argv[1] if len(sys.argv)>1 else 'http://127.0.0.1:9080'
def client():
 jar=CookieJar();return build_opener(ProxyHandler({}),HTTPCookieProcessor(jar)),jar
def call(o,path,method='GET',body=None,timeout=5):
 data=None if body is None else json.dumps(body).encode()
 last=None
 for attempt in range(5):
  req=Request(BASE+path,data=data,method=method,headers={'Content-Type':'application/json','Connection':'close'})
  try:
   with o.open(req,timeout=timeout) as r:
    raw=r.read().decode();return r.status,json.loads(raw) if raw else None
  except HTTPError as e:
   raw=e.read().decode();
   try:value=json.loads(raw) if raw else None
   except:value=raw
   return e.code,value
  except (URLError,ConnectionError,TimeoutError,OSError) as e:
   last=e
   if attempt<4:time.sleep(.25*(attempt+1))
 if last:raise last
def wait_state(o,lifecycle,seconds=8):
 end=time.time()+seconds
 while time.time()<end:
  status,state=call(o,'/state')
  if status==200 and state['lifecycle']==lifecycle:return state
  time.sleep(.1)
 raise AssertionError(f'expected {lifecycle}, last={state}')
def operation(o,path,corr):
 status,_=call(o,path,'POST',{'correlationId':corr});assert status==202
 end=time.time()+5
 while time.time()<end:
  status,result=call(o,f'/request-result?correlationId={corr}')
  if status==200:return result
  time.sleep(.05)
 raise AssertionError('request result timeout')
def fixture(o,action,expected=200):
 status,value=call(o,'/fixture','POST',{'action':action});assert status==expected,(status,value);return value
master,master_jar=client();spectator,spectator_jar=client()
assert call(master,'/health')[0]==200
for o,j in ((master,master_jar),(spectator,spectator_jar)):
 status,c=call(o,'/context');assert status==200 and c['role']=='Spectator' and len(list(j))==1
assert call(master,'/bootstrap','POST')[1]['bootstrap'] is True
assert call(master,'/context')[1]['role']=='Race Director'
assert call(spectator,'/context')[1]['role']=='Spectator'
assert call(spectator,'/bootstrap','POST')[0]==409
assert operation(spectator,'/request/race-again',12)['result']=='REJECTED'
assert call(master,'/request-result?correlationId=12')[0]==204
assert operation(master,'/request/start',1)['result']=='ACCEPTED';wait_state(master,'RACING')
fixture(master,'lane1');time.sleep(.25);fixture(master,'lane2');time.sleep(.25);fixture(master,'lane1');finished=wait_state(master,'FINISHED')
assert finished['resultSealed'] and finished['winningTime']==finished['finishTime']
status,results=call(master,'/results');assert status==200 and results['sealed'] and len(results['entries'])==2
status,details=call(master,'/details');assert status==200 and details['sealed'] and any(e['records'] for e in details['entries'])
assert call(spectator,'/results')[1]==results and call(spectator,'/details')[1]==details
before=json.dumps(results,sort_keys=True);fixture(master,'lane1',400);assert json.dumps(call(master,'/results')[1],sort_keys=True)==before
assert operation(spectator,'/request/race-again',22)['result']=='REJECTED';assert call(master,'/request-result?correlationId=22')[0]==204
assert operation(master,'/request/race-again',2)['result']=='ACCEPTED';ready=wait_state(master,'READY');assert ready['resultSealed']
fixture(master,'target3');old_ids=[e['raceEntryId'] for e in results['entries']]
assert operation(master,'/request/start',3)['result']=='ACCEPTED';fresh=wait_state(master,'RACING');assert not fresh['resultSealed'] and all(e['laps']==0 for e in fresh['entries']) and min(e['raceEntryId'] for e in fresh['entries'])>min(old_ids)
assert call(spectator,'/context')[1]['role']=='Spectator'
# Leave the live human checkpoint in a repeatable READY state while retaining
# the Browser identity that deliberately acquired Race Director authority.
fixture(master,'reset');clean=wait_state(master,'READY')
assert call(master,'/context')[1]['role']=='Race Director'
assert not clean['resultSealed'] and all(e['laps']==0 for e in clean['entries'])
assert call(master,'/fact')[1]['type']=='NONE'
print('Stage 12 real HTTP Browser acceptance PASS')
