const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.resolve(__dirname, '..', '..');
const checkedInPath = path.join(ROOT, 'firmware', 'include', 'pp', 'normal_browser_page.inc');
const embedded = fs.readFileSync(checkedInPath, 'utf8');
const match = embedded.match(/R"PPHTML\(([\s\S]*)\)PPHTML";/);
if (!match) throw new Error('embedded normal Browser payload missing');
const scriptMatch = match[1].match(/<script>([\s\S]*)<\/script>/);
if (!scriptMatch) throw new Error('embedded normal Browser script missing');

class Element {
  constructor(id) { this.id=id; this.textContent=''; this._innerHTML=''; this.value=''; this.disabled=false; this.hidden=false; this.onclick=null; this.listeners={}; this.classList={toggle(){},add(){},remove(){}}; } addEventListener(type,fn,options){this.listeners[type]={fn,options};}
  set innerHTML(v) {
    this._innerHTML=String(v);
    for (const x of this._innerHTML.matchAll(/id=["']([^"']+)["']/g)) if (!document.elements[x[1]]) document.elements[x[1]]=new Element(x[1]);
    for (const x of this._innerHTML.matchAll(/class=["'][^"']*historyItem[^"']*["'][^>]*data-sequence=["']([^"']+)["']/g)) {
      const id=`historyItem_${x[1]}`; const e=document.elements[id]||new Element(id); e.dataset={sequence:x[1]}; document.elements[id]=e;
    }
  }
  get innerHTML(){return this._innerHTML;} insertAdjacentHTML(_position,html){this._innerHTML+=String(html);}
}
const ids=['logo','head','status','reason','app','overlay','notice','setupNav','resultsNav','trackNav','racersNav','settingsNav','start','change','devTab','devDrawer','devClose','devL1','devL2','devState','devLanes','devFact','devHealth','devHealthMore','devFixture'];
global.document={elements:{},querySelector(sel){if(sel.startsWith('#')) return this.elements[sel.slice(1)]||null; if(sel==='.historyItem') return Object.values(this.elements).find(e=>e.id.startsWith('historyItem_'))||null; return null;},querySelectorAll(sel){if(sel==='.historyItem') return Object.values(this.elements).filter(e=>e.id.startsWith('historyItem_')); return []}};
for(const id of ids) document.elements[id]=new Element(id);
global.window=global;global.$=s=>document.querySelector(s);global.performance={now:()=>1000};
const intervals=[];global.setInterval=fn=>{intervals.push(fn);return intervals.length};global.clearInterval=()=>{};
let nextTimer=1;const timers=new Map();global.setTimeout=(fn,ms)=>{const id=nextTimer++;timers.set(id,{fn,ms});return id};global.clearTimeout=id=>timers.delete(id);
let state={lifecycle:'READY',sessionMode:'NONE',resultSealed:false,practiceSummaryAvailable:false,historySequence:0,entries:[]};
const proposal={mode:'LAP_RACE',lapTarget:2,durationMinutes:10,finishBehaviour:'IMMEDIATE',startable:true,proposalRevision:1,entries:[]};
const context={hasMaster:true,role:'Race Director'};
let resultRequests=0,historyRequests=0,detailsRequests=0,practiceAgainRequests=0,practiceSummaryFixture={available:true,sessionFastestLap:1200000,entries:[{raceEntryId:1,lane:1,laps:3,bestLap:1200000}]},failCurrent=false,serveCommitted=false;
function response(value,status=200){return {ok:status>=200&&status<300,status,async json(){return JSON.parse(JSON.stringify(value));}};}
const currentResult={sequence:1,sealed:true,mode:'LAP_RACE',lapTarget:10,fastestLap:1200000,entries:[{rank:1,mugId:91,lane:1,classifiedLaps:10,completed:true,completionTime:5000000000,finishDurationUs:2500000,bestLap:1200000,lapsBehind:0},{rank:2,mugId:92,lane:2,classifiedLaps:1,completed:false,completionTime:0,finishDurationUs:3400000,bestLap:3400000,lapsBehind:9}]};
const historical={sequence:1,sealed:true,mode:'LAP_RACE',lapTarget:2,fastestLap:1200000,entries:[{rank:1,mugId:91,lane:1,completed:true,completionTime:5000000000,finishDurationUs:2500000,bestLap:1200000}]};
const committedResult={sequence:2,sealed:true,mode:'LAP_RACE',lapTarget:10,fastestLap:1200000,entries:[{rank:1,mugId:91,lane:1,classifiedLaps:10,completed:true,completionTime:5000000000,finishDurationUs:2500000,bestLap:1200000,lapsBehind:0},{rank:2,mugId:92,lane:2,classifiedLaps:1,completed:false,completionTime:0,finishDurationUs:3400000,bestLap:3400000,lapsBehind:9}]};
const enduranceResult={sequence:2,sealed:true,valid:true,mode:'ENDURANCE',durationMinutes:1,durationUs:60000000,finishBehaviour:'COMPLETE_CURRENT_LAP',overtime:true,overtimeUs:12490000,fastestLap:950000,entries:[{rank:1,mugId:91,lane:1,laps:20,classifiedLaps:20,lapPenalty:0,tied:false,completed:true,completionTime:204070000,finishDurationUs:0,bestLap:950000},{rank:2,mugId:92,lane:2,laps:20,classifiedLaps:20,lapPenalty:0,tied:false,completed:true,completionTime:208190000,finishDurationUs:0,bestLap:970000}]};
global.fetch=async(url,options={})=>{
  if(url==='/state') return response(state);
  if(url==='/proposal') return response(proposal);
  if(url==='/context') return response(context);
  if(url==='/results'){resultRequests++; if(failCurrent)return response({},503); if(state.resultSealed)return response(state.sessionMode==='ENDURANCE'?enduranceResult:(serveCommitted?committedResult:currentResult)); return response({},261);}
  if(url==='/results?sequence=1'){resultRequests++;return response(historical);}
  if(url==='/history'){historyRequests++;return response({entries:[{sequence:1,mode:'LAP_RACE',lapTarget:2,entries:[{mugId:91,lane:1}]}]});}
  if(url==='/details'||url==='/details?sequence=1'){detailsRequests++;return response({entries:[]});}
  if(url==='/practice-summary')return response(practiceSummaryFixture);
  if(url==='/request/practice-again'){practiceAgainRequests++;state={...state,lifecycle:'READY',sessionMode:'NONE',practiceSummaryAvailable:false,resultSealed:false};return response({},202);}
  if(url.startsWith('/request-result?'))return response({result:'ACCEPTED'});
  return response({},202);
};
vm.runInThisContext(scriptMatch[1],{filename:'embedded-normal-browser.js'});
const sleep=async()=>{for(let i=0;i<12;i++)await Promise.resolve();};
const expect=(v,m)=>{if(!v)throw new Error(m)};
(async()=>{
 await sleep();
 document.elements.resultsNav.onclick(); await sleep();
 expect(resultRequests===1,'unavailable current Results was not requested exactly once');
 for(let i=0;i<4;i++){await poll();await sleep();}
 expect(resultRequests===1,'unavailable current Results was repeatedly requested while READY');
 state={...state,lifecycle:'FINISHED',resultSealed:true,historySequence:1}; await poll(); await sleep();
 document.elements.resultsNav.onclick(); await sleep(); expect(resultRequests===3,'current Results was not fetched when legitimately available');
 expect(document.elements.app.innerHTML.includes('PROGRESS')&&document.elements.app.innerHTML.includes('TIME')&&document.elements.app.innerHTML.includes('BEST')&&document.elements.app.innerHTML.includes('GAP'),'Lap Results did not render the frozen column headings');
expect(document.elements.app.innerHTML.includes('10 / 10')&&document.elements.app.innerHTML.includes('1 / 10'),'Lap Results did not render authoritative classified progress');
expect(document.elements.app.innerHTML.includes('2.50 s')&&document.elements.app.innerHTML.includes('3.40 s'),'Lap Results did not show winner and completed-progress time');
expect(document.elements.app.innerHTML.includes('+9 laps'),'Lap Results did not render authoritative lap gap');
expect(!document.elements.app.innerHTML.includes('>RESULT<'),'Lap Results retained the ambiguous RESULT column');
expect(document.elements.app.innerHTML.includes('2.50 s'),'Results did not render the authoritative race-relative finish duration');
 expect(!document.elements.app.innerHTML.includes('5000.00 s'),'Results rendered the raw boot-relative completion timestamp as elapsed duration');
// A just-finished current result can be visible before HistoryStored. The
// Browser must invalidate that provisional response when the committed history
// sequence appears, then fetch the authoritative completed values.
state={...state,historySequence:0}; currentResult.entries[0].finishDurationUs=0; currentResult.entries[0].bestLap=0; currentResult.fastestLap=0; resultData=null; resultDataKey=null; resultDataHistorySequence=null; document.elements.resultsNav.onclick(); await sleep(); expect(document.elements.app.innerHTML.includes('—'),'provisional Results did not show unavailable timing'); serveCommitted=true; state={...state,historySequence:2}; await poll(); await sleep(); await sleep(); expect(resultRequests>=4&&document.elements.app.innerHTML.includes('2.50 s')&&document.elements.app.innerHTML.includes('1.20 s')&&document.elements.app.innerHTML.includes('Fastest Lap · 1.20 s'),'Results did not refetch the committed History payload after HistoryStored'); serveCommitted=false;
 const lapFinishVariants=[
  {finishBehaviour:'IMMEDIATE',entries:[{rank:1,mugId:91,lane:1,classifiedLaps:10,completed:true,finishDurationUs:2500000,bestLap:1200000,lapsBehind:0},{rank:2,mugId:92,lane:2,classifiedLaps:1,completed:false,finishDurationUs:0,bestLap:1400000,lapsBehind:9}]},
  {finishBehaviour:'COMPLETE_CURRENT_LAP',entries:[{rank:1,mugId:91,lane:1,classifiedLaps:10,completed:true,finishDurationUs:2500000,bestLap:1200000,lapsBehind:0},{rank:2,mugId:92,lane:2,classifiedLaps:7,completed:true,finishDurationUs:3600000,bestLap:1400000,lapsBehind:3}]},
  {finishBehaviour:'COMPLETE_FULL_RACE_DISTANCE',entries:[{rank:1,mugId:91,lane:1,classifiedLaps:10,completed:true,finishDurationUs:2500000,bestLap:1200000,lapsBehind:0},{rank:2,mugId:92,lane:2,classifiedLaps:10,completed:true,finishDurationUs:5200000,bestLap:1400000,lapsBehind:0}]}
 ];
 for(const variant of lapFinishVariants){currentResult.finishBehaviour=variant.finishBehaviour;currentResult.entries=variant.entries;resultData=null;resultDataKey=null;document.elements.resultsNav.onclick=()=>{selectedHistorySequence=null;resultData=null;resultDataKey=null;V='results';render()};document.elements.resultsNav.onclick();await sleep();const html=document.elements.app.innerHTML;expect(html.includes('10 / 10')&&html.includes('PROGRESS'),'Lap Results lost authoritative progress for '+variant.finishBehaviour);expect(html.includes('2.50 s')&&html.includes('BEST')&&html.includes('GAP'),'Lap Results lost winner time/best/gap for '+variant.finishBehaviour);if(variant.finishBehaviour==='IMMEDIATE')expect(html.includes('—'),'Immediate zero-progress racer received a fabricated finish time');else expect(html.includes('3.60 s')||html.includes('5.20 s'),'Permitted finisher did not receive its own finish time');}
 currentResult.finishBehaviour='IMMEDIATE';
currentResult.entries[1].completed=true; currentResult.entries[1].finishDurationUs=3600000; currentResult.entries[1].classifiedLaps=10; currentResult.entries[1].lapsBehind=0; resultData=null; resultDataKey=null; document.elements.resultsNav.onclick(); await sleep(); expect(document.elements.app.innerHTML.includes('3.60 s'),'Finish Current Lap/full-distance finisher did not show its own authoritative time');
currentResult.entries[1].completed=false; currentResult.entries[1].finishDurationUs=3400000; currentResult.entries[1].bestLap=3400000; currentResult.entries[1].classifiedLaps=7; currentResult.entries[1].lapsBehind=3; resultData=null; resultDataKey=null; document.elements.resultsNav.onclick(); await sleep(); expect(document.elements.app.innerHTML.includes('7 / 10')&&document.elements.app.innerHTML.includes('+3 laps')&&document.elements.app.innerHTML.includes('3.40 s'),'Stopped competitor did not retain progress and completed-progress time');
 const currentResultsRequestBaseline=resultRequests; for(let i=0;i<4;i++){await poll();await sleep();}
 expect(resultRequests===currentResultsRequestBaseline,'current Results was refetched on every poll');
 state={...state,sessionMode:'ENDURANCE'}; resultData=null; resultDataKey=null; await poll(); await sleep(); document.elements.resultsNav.onclick(); await sleep();
 expect(document.elements.app.innerHTML.includes('20 laps')&&document.elements.app.innerHTML.includes('20 classified'),'Endurance Results did not present authoritative factual/classified laps');
 expect(document.elements.app.innerHTML.includes('1 min')&&document.elements.app.innerHTML.includes('Finish current lap'),'Endurance Results did not present authoritative duration/finish policy');
 expect(document.elements.app.innerHTML.includes('Overtime')&&document.elements.app.innerHTML.includes('+00:12.4'),'Endurance Results did not present authoritative overtime');
 expect(!document.elements.app.innerHTML.includes('204.07 s')&&!document.elements.app.innerHTML.includes('208.19 s'),'Endurance Results exposed raw completionTime as elapsed RESULT');
 enduranceResult.entries[0].laps=1; enduranceResult.entries[0].classifiedLaps=1; enduranceResult.entries[1].laps=0; enduranceResult.entries[1].classifiedLaps=0; resultData=null; resultDataKey=null; document.elements.resultsNav.onclick(); await sleep();
 expect(document.elements.app.innerHTML.includes('1 lap')&&!document.elements.app.innerHTML.includes('1 laps'),'Results did not singularize one lap');
 expect(document.elements.app.innerHTML.includes('0 laps'),'Results did not pluralize zero laps');
 state={lifecycle:'RACING',sessionMode:'ENDURANCE',resultSealed:false,practiceSummaryAvailable:false,finishBehaviour:'COMPLETE_CURRENT_LAP',remainingDuration:0,durationExpired:true,overtime:12400000,entries:[]};
 await poll(); await sleep();
 expect(document.elements.app.innerHTML.includes('Overtime:')&&document.elements.app.innerHTML.includes('+00:12.4'),'live Endurance overtime was not rendered from authoritative state');
 state={...state,lifecycle:'FINISHED',resultSealed:true,historySequence:1}; await poll(); await sleep(); document.elements.resultsNav.onclick(); await sleep();
 document.querySelector('#history').onclick(); await sleep(); expect(historyRequests===1,'History navigation did not request History');
 const historicalRequestBaseline=resultRequests; document.querySelector('.historyItem').onclick(); await sleep(); expect(resultRequests===historicalRequestBaseline+1,'selected historical Results was not requested');
 for(let i=0;i<4;i++){await poll();await sleep();}
 expect(resultRequests===historicalRequestBaseline+1,'selected historical Results was not stable through polling');
 failCurrent=true; const failedRequestBaseline=resultRequests; document.elements.resultsNav.onclick(); await sleep();
 expect(resultRequests===failedRequestBaseline+1,'server-error Results request was not attempted');
 expect(document.elements.notice.textContent.includes('HTTP 503'),'server-error status was hidden from the user');
 document.elements.setupNav.onclick(); await sleep(); expect(!document.elements.app.innerHTML.includes('RESULTS</div><h1>'),'navigation away from Results did not render setup');
 state={lifecycle:'READY',sessionMode:'NONE',resultSealed:false,practiceSummaryAvailable:false,historySequence:1,entries:[]};
 await poll(); await sleep(); expect(resultRequests===failedRequestBaseline+1,'READY after abandonment/race-again resumed current-result retrieval');
 state={lifecycle:'RACING',sessionMode:'LAP_RACE',resultSealed:false,practiceSummaryAvailable:false,entries:[]}; await poll(); await sleep(); expect(document.elements.app.innerHTML.includes('id="pause"')&&!document.elements.app.innerHTML.includes('id="endRace"')&&!document.elements.app.innerHTML.includes('id="endPractice"'),'active Lap Race did not expose PAUSE only');
 state={lifecycle:'RACING',sessionMode:'ENDURANCE',resultSealed:false,practiceSummaryAvailable:false,entries:[]}; await poll(); await sleep(); expect(document.elements.app.innerHTML.includes('id="pause"')&&!document.elements.app.innerHTML.includes('id="endRace"')&&!document.elements.app.innerHTML.includes('id="endPractice"'),'active Endurance did not expose PAUSE only');
 state={lifecycle:'PAUSED',sessionMode:'LAP_RACE',resultSealed:false,practiceSummaryAvailable:false,entries:[]}; await poll(); await sleep(); expect(document.elements.overlay.innerHTML.includes('id="resume"')&&document.elements.overlay.innerHTML.includes('id="restart"')&&document.elements.overlay.innerHTML.includes('id="abandon"')&&!document.elements.overlay.innerHTML.includes('id="endPractice"'),'paused competitive race did not expose RESUME/RESTART/ABANDON');
 state={lifecycle:'PAUSED',sessionMode:'ENDURANCE',resultSealed:false,practiceSummaryAvailable:false,entries:[]}; await poll(); await sleep(); expect(document.elements.overlay.innerHTML.includes('id="resume"')&&document.elements.overlay.innerHTML.includes('id="restart"')&&document.elements.overlay.innerHTML.includes('id="abandon"')&&!document.elements.overlay.innerHTML.includes('id="endPractice"'),'paused Endurance did not expose RESUME/RESTART/ABANDON');
 state={lifecycle:'RACING',sessionMode:'OPEN_PRACTICE',resultSealed:false,practiceSummaryAvailable:false,entries:[]}; await poll(); await sleep(); expect(document.elements.app.innerHTML.includes('id="pause"')&&!document.elements.app.innerHTML.includes('id="endRace"')&&!document.elements.app.innerHTML.includes('id="endPractice"'),'active Practice did not expose PAUSE only');
 state={lifecycle:'PAUSED',sessionMode:'OPEN_PRACTICE',resultSealed:false,practiceSummaryAvailable:false,entries:[]}; await poll(); await sleep(); expect(document.elements.overlay.innerHTML.includes('id="resume"')&&document.elements.overlay.innerHTML.includes('id="restart"')&&document.elements.overlay.innerHTML.includes('id="endPractice"')&&!document.elements.overlay.innerHTML.includes('id="abandon"'),'paused Practice did not expose RESUME/RESTART/END PRACTICE');
 state={lifecycle:'READY',sessionMode:'NONE',resultSealed:false,practiceSummaryAvailable:true,entries:[]}; await poll(); await sleep(); expect(document.elements.app.innerHTML.includes('PRACTICE SUMMARY')&&document.elements.app.innerHTML.includes('PRACTICE AGAIN')&&document.elements.app.innerHTML.includes('HOME')&&!document.elements.app.innerHTML.includes('RESUME'),'Practice Summary did not expose only PRACTICE AGAIN and HOME');
document.elements.again.onclick(); await sleep(); await sleep(); expect(practiceAgainRequests===1,'PRACTICE AGAIN did not use the authoritative practice-again request'); expect(state.lifecycle==='READY'&&!state.practiceSummaryAvailable,'PRACTICE AGAIN did not reconstruct authoritative READY'); document.elements.setupNav.onclick(); await sleep(); expect(document.elements.app.innerHTML.includes('RACE SETUP'),'PRACTICE AGAIN did not leave Summary at setup/READY');
state={lifecycle:'READY',sessionMode:'NONE',resultSealed:false,practiceSummaryAvailable:true,entries:[]}; practiceSummaryFixture={available:true,sessionFastestLap:1200000,entries:[]}; await poll(); await sleep(); document.elements.homePractice.onclick(); await sleep(); expect(!document.elements.app.innerHTML.includes('PRACTICE SUMMARY'),'HOME did not leave Practice Summary');
failCurrent=false; state={lifecycle:'RACING',sessionMode:'LAP_RACE',resultSealed:false,practiceSummaryAvailable:false,entries:[]}; await poll(); await sleep();
 state={lifecycle:'FINISHED',sessionMode:'LAP_RACE',resultSealed:true,practiceSummaryAvailable:false,historySequence:1,entries:[]}; await poll(); await sleep();
 expect(document.elements.app.innerHTML.includes('>RESULTS</div>'),'FINISHED authoritative result did not render Results automatically');
 document.elements.setupNav.onclick(); await sleep(); await poll(); await sleep(); expect(document.elements.app.innerHTML.includes('RACE SETUP'),'Race Setup navigation was overridden by FINISHED polling');
 document.elements.trackNav.onclick(); await sleep(); expect(document.elements.app.innerHTML.includes('Track Setup'),'Track Setup navigation did not render');
 document.elements.racersNav.onclick(); await sleep(); expect(document.elements.app.innerHTML.includes('Racers'),'Racers navigation did not render');
 document.elements.settingsNav.onclick(); await sleep(); expect(document.elements.app.innerHTML.includes('Settings'),'Settings navigation did not render');
 document.elements.resultsNav.onclick(); await sleep(); expect(document.elements.app.innerHTML.includes('RESULTS'),'returning to Results did not render');
 console.log('Stage 14C normal Browser Results runtime PASS: unavailable-result loop bounded, current and historical Results cached/stable, navigation and abandonment recovery work, active and paused termination controls match authoritative mode/lifecycle policy');
})().catch(e=>{console.error('Stage 14C normal Browser Results runtime FAIL:',e.stack||e.message);process.exitCode=1});
