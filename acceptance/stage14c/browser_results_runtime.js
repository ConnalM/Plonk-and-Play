const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.resolve(__dirname, '..', '..');
const embedded = fs.readFileSync(path.join(ROOT, 'firmware', 'include', 'pp', 'normal_browser_page.inc'), 'utf8');
const match = embedded.match(/R"PPHTML\(([\s\S]*)\)PPHTML";/);
if (!match) throw new Error('embedded normal Browser payload missing');
const scriptMatch = match[1].match(/<script>([\s\S]*)<\/script>/);
if (!scriptMatch) throw new Error('embedded normal Browser script missing');

class Element {
  constructor(id) { this.id=id; this.textContent=''; this._innerHTML=''; this.value=''; this.disabled=false; this.hidden=false; this.onclick=null; this.classList={toggle(){},add(){},remove(){}}; }
  set innerHTML(v) {
    this._innerHTML=String(v);
    for (const x of this._innerHTML.matchAll(/id=["']([^"']+)["']/g)) if (!document.elements[x[1]]) document.elements[x[1]]=new Element(x[1]);
    for (const x of this._innerHTML.matchAll(/class=["'][^"']*historyItem[^"']*["'][^>]*data-sequence=["']([^"']+)["']/g)) {
      const id=`historyItem_${x[1]}`; const e=document.elements[id]||new Element(id); e.dataset={sequence:x[1]}; document.elements[id]=e;
    }
  }
  get innerHTML(){return this._innerHTML;}
}
const ids=['logo','head','status','reason','app','overlay','notice','setupNav','resultsNav','start','change','devTab','devDrawer','devClose','devL1','devL2','devState','devLanes','devFact','devHealth','devHealthMore','devFixture'];
global.document={elements:{},querySelector(sel){if(sel.startsWith('#')) return this.elements[sel.slice(1)]||null; if(sel==='.historyItem') return Object.values(this.elements).find(e=>e.id.startsWith('historyItem_'))||null; return null;},querySelectorAll(sel){if(sel==='.historyItem') return Object.values(this.elements).filter(e=>e.id.startsWith('historyItem_')); return []}};
for(const id of ids) document.elements[id]=new Element(id);
global.window=global;global.$=s=>document.querySelector(s);global.performance={now:()=>1000};
const intervals=[];global.setInterval=fn=>{intervals.push(fn);return intervals.length};global.clearInterval=()=>{};
let nextTimer=1;const timers=new Map();global.setTimeout=(fn,ms)=>{const id=nextTimer++;timers.set(id,{fn,ms});return id};global.clearTimeout=id=>timers.delete(id);
let state={lifecycle:'READY',sessionMode:'NONE',resultSealed:false,practiceSummaryAvailable:false,historySequence:0,entries:[]};
const proposal={mode:'LAP_RACE',lapTarget:2,durationMinutes:10,finishBehaviour:'IMMEDIATE',startable:true,proposalRevision:1,entries:[]};
const context={hasMaster:true,role:'Race Director'};
let resultRequests=0,historyRequests=0,detailsRequests=0,failCurrent=false;
function response(value,status=200){return {ok:status>=200&&status<300,status,async json(){return JSON.parse(JSON.stringify(value));}};}
const currentResult={sequence:1,sealed:true,mode:'LAP_RACE',lapTarget:2,fastestLap:1200000,entries:[{rank:1,mugId:91,lane:1,completed:true,completionTime:5000000,bestLap:1200000}]};
const historical={sequence:1,sealed:true,mode:'LAP_RACE',lapTarget:2,fastestLap:1200000,entries:[{rank:1,mugId:91,lane:1,completed:true,completionTime:5000000,bestLap:1200000}]};
global.fetch=async(url,options={})=>{
  if(url==='/state') return response(state);
  if(url==='/proposal') return response(proposal);
  if(url==='/context') return response(context);
  if(url==='/results'){resultRequests++; if(failCurrent)return response({},503); if(state.resultSealed)return response(currentResult); return response({},261);}
  if(url==='/results?sequence=1'){resultRequests++;return response(historical);}
  if(url==='/history'){historyRequests++;return response({entries:[{sequence:1,mode:'LAP_RACE',lapTarget:2,entries:[{mugId:91,lane:1}]}]});}
  if(url==='/details'||url==='/details?sequence=1'){detailsRequests++;return response({entries:[]});}
  if(url==='/practice-summary')return response({available:false});
  return response({},202);
};
vm.runInThisContext(scriptMatch[1],{filename:'embedded-normal-browser.js'});
const sleep=async()=>{for(let i=0;i<12;i++)await Promise.resolve();};
const expect=(v,m)=>{if(!v)throw new Error(m)};
(async()=>{
 await sleep();
 document.elements.resultsNav.onclick(); await sleep();
 expect(resultRequests===1,'unavailable current Results was not requested exactly once');
 for(let i=0;i<4;i++){await intervals[0]();await sleep();}
 expect(resultRequests===1,'unavailable current Results was repeatedly requested while READY');
 state={...state,lifecycle:'FINISHED',resultSealed:true,historySequence:1};
 document.elements.resultsNav.onclick(); await sleep(); expect(resultRequests===2,'current Results was not fetched when legitimately available');
 for(let i=0;i<4;i++){await intervals[0]();await sleep();}
 expect(resultRequests===2,'current Results was refetched on every poll');
 document.querySelector('#history').onclick(); await sleep(); expect(historyRequests===1,'History navigation did not request History');
 document.querySelector('.historyItem').onclick(); await sleep(); expect(resultRequests===3,'selected historical Results was not requested');
 for(let i=0;i<4;i++){await intervals[0]();await sleep();}
 expect(resultRequests===3,'selected historical Results was not stable through polling');
 failCurrent=true; document.elements.resultsNav.onclick(); await sleep();
 expect(resultRequests===4,'server-error Results request was not attempted');
 expect(document.elements.notice.textContent.includes('HTTP 503'),'server-error status was hidden from the user');
 document.elements.setupNav.onclick(); await sleep(); expect(!document.elements.app.innerHTML.includes('RESULTS</div><h1>'),'navigation away from Results did not render setup');
 state={lifecycle:'READY',sessionMode:'NONE',resultSealed:false,practiceSummaryAvailable:false,historySequence:1,entries:[]};
 await intervals[0](); await sleep(); expect(resultRequests===4,'READY after abandonment/race-again resumed current-result retrieval');
 state={lifecycle:'RACING',sessionMode:'LAP_RACE',resultSealed:false,practiceSummaryAvailable:false,entries:[]}; await intervals[0](); await sleep(); expect(!document.elements.app.innerHTML.includes('id="endRace"'),'Lap Race offered invalid active END RACE');
 state={lifecycle:'RACING',sessionMode:'ENDURANCE',resultSealed:false,practiceSummaryAvailable:false,entries:[]}; await intervals[0](); await sleep(); expect(document.elements.app.innerHTML.includes('id="endRace"'),'Endurance did not offer active END RACE');
 console.log('Stage 14C normal Browser Results runtime PASS: unavailable-result loop bounded, current and historical Results cached/stable, navigation and abandonment recovery work, Lap/Endurance END RACE exposure correct');
})().catch(e=>{console.error('Stage 14C normal Browser Results runtime FAIL:',e.stack||e.message);process.exitCode=1});
