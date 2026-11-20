// Timed Stage source-level regression harness; no DOM/layout engine.
// Run: node PP_Timed_Stage_regression.cjs
const fs=require('node:fs'),path=require('node:path');

function testPrototype(script){
const rows=[],els={},handlers={},timers=new Map();let nextId=0;
function get(id){return els[id]??=( {value:id==='size'?'desktop':'',innerHTML:'',textContent:'',dataset:{},querySelectorAll:()=>[]} );}
const document={getElementById:get,querySelectorAll:()=>[],addEventListener:(n,fn)=>handlers[n]=fn};
const setTimeout=(fn,delay)=>{timers.set(++nextId,{fn,delay});return nextId},clearTimeout=id=>timers.delete(id);
const api=new Function('document','setTimeout','clearTimeout','alert',script+'\nreturn {buildQueue,loadSlot,setSnapshot,render,setup,run,review,classification,matrix,aux,splits,mapping,fixtures,change:(k,v)=>{switch(k){case "eventOrder":eventOrder=v;break;case "attemptOrder":attemptOrder=v;break;case "multi":multi=v;break;case "falsePolicy":falsePolicy=v;break;case "hold":hold=v;break;case "role":role=v;break;case "stageIndex":stageIndex=v;break;case "route":route=v;break;}},state:()=>({snapshot,route,active,cursor,stageIndex,racer,attempt,queue,completed,falseSeen,reviewPenalty}), startup:()=>({hold,falsePolicy,eventOrder,attemptOrder}),click:a=>document._click(a)};')(document,setTimeout,clearTimeout,()=>{});
document._click=a=>handlers.click({target:{closest:()=>({dataset:{action:a},disabled:false})}});
function check(name,fn){try{rows.push({name,status:fn()?'PASS':'FAIL'})}catch(e){rows.push({name,status:'ERROR',detail:String(e)})}}
const q=()=>api.buildQueue().map(x=>[x.stage,x.racer,x.attempt].join('')).join(',');
check('default sporting settings',()=>JSON.stringify(api.startup())===JSON.stringify({hold:3,falsePolicy:'warning',eventOrder:'stage',attemptOrder:'round'}));
check('default round ordering',()=>q()==='001,011,021,002,012,022,101,111,121');
api.change('attemptOrder','racer');check('racer attempt ordering',()=>q()==='001,002,011,012,021,022,101,111,121');
api.change('eventOrder','mug');check('MUG order fixture',()=>q()==='001,002,101,011,012,111,021,022,121');
api.change('eventOrder','stage');api.change('attemptOrder','round');api.change('multi',false);check('single stage has six slots',()=>api.buildQueue().length===6);api.change('multi',true);
check('loop sensor mapping',()=>api.mapping({type:'loop'}).includes('S/F'));check('A to B mapping',()=>api.mapping({type:'ab'}).includes('Sensor 1 → B · Sensor 2'));check('B to A mapping',()=>api.mapping({type:'ba'}).includes('Sensor 2 → A · Sensor 1'));
api.click('ready');check('READY starts queue at slot one',()=>api.state().active&&api.state().cursor===0&&api.state().snapshot==='ready');
api.click('countdown');check('START enters countdown',()=>api.state().snapshot==='countdown'&&api.run().includes('00:00'));
api.setSnapshot('armed');check('GO retains timer zero',()=>api.run().includes('00:00')&&api.run().includes('GO · ARMED'));
api.setSnapshot('running');check('running fixture clock',()=>api.run().includes('00:26'));
api.setSnapshot('split');check('split precision and three second scheduled return',()=>api.run().includes('00:23.45')&&[...timers.values()].some(t=>t.delay===3000));
let t=[...timers.values()][0];t.fn();check('split returns to running snapshot',()=>api.state().snapshot==='afterSplit'&&api.run().includes('00:48')&&api.run().includes('00:23.45'));
api.change('hold',5);api.setSnapshot('split');check('configurable split hold',()=>[...timers.values()].some(t=>t.delay===5000));
api.setSnapshot('finish');check('finish cancels pending hold',()=>timers.size===0&&api.run().includes('01:12.34'));
check('valid finish cannot retry',()=>!api.review().includes('RETRY ATTEMPT'));api.click('retry');check('retry action rejects completed finish',()=>api.state().snapshot==='finish');
api.click('next');check('accept advances to second racer',()=>api.state().cursor===1&&api.state().racer===1&&api.state().completed.length===1);
api.setSnapshot('dnf');check('DNF has retry option',()=>api.review().includes('RETRY ATTEMPT'));api.click('retry');check('retry stays in same slot',()=>api.state().cursor===1&&api.state().completed.length===1&&api.state().snapshot==='retry');
api.change('falsePolicy','penalty');api.setSnapshot('false');check('false-start penalty visible',()=>api.run().includes('5 s'));api.click('armed');api.setSnapshot('finish');check('raw and adjusted separate',()=>api.review().includes('01:12.34')&&api.review().includes('01:17.34')&&api.review().includes('5.00 s'));
api.click('next');api.change('falsePolicy','dnf');api.setSnapshot('false');check('invalid-run action offered',()=>api.run().includes('CONFIRM DNF'));api.click('dnf');check('invalid attempt becomes DNF',()=>api.state().snapshot==='dnf');
api.click('next');check('DNF does not stop sequence',()=>api.state().cursor===3&&api.state().active);
for(let i=0;i<3;i++){api.setSnapshot('finish');api.click('next');}
check('stage boundary reached after six accepted slots',()=>api.state().cursor===6&&api.state().route==='stage');
check('auxiliary stage classification represented',()=>api.aux().includes('Position')&&api.aux().includes('Gap'));
api.click('nextStage');check('NEXT STAGE loads second stage',()=>api.state().stageIndex===1&&api.state().snapshot==='ready');
check('point to point suppresses lap splits',()=>api.splits(api.fixtures.finish).includes('no lap splits'));
api.change('role','mug');api.click('countdown');check('MUG cannot start stage',()=>api.state().snapshot==='ready');api.change('role','director');
check('auxiliary no race action buttons',()=>!api.aux().includes('data-action'));
check('classification fixture disclosed',()=>api.classification(false).includes('not calculated from setup'));check('matrix expandable splits',()=>api.matrix().includes('<details'));
check('auxiliary returns to live timing after NEXT STAGE',()=>!api.aux().includes('<th>Position</th>')&&api.aux().includes('READY'));
check('active race accessible after navigating to setup',()=>{api.change('route','setup');return api.setup().includes('RESUME')||api.setup().includes('RETURN TO RUN');});
api.click('returnRun');check('return action restores active run',()=>api.state().route==='run');api.change('route','run');api.setSnapshot('finish');api.click('next');api.setSnapshot('finish');api.click('next');api.setSnapshot('finish');api.click('next');check('all slots accepted and event inactive',()=>api.state().completed.length===9&&!api.state().active);
check('auxiliary final event classification represented',()=>api.aux().includes('Event results')&&api.aux().includes('Gap'));
return rows;
}

const html=fs.readFileSync(path.join(__dirname,'PP_Timed_Stage_review.html'),'utf8');
const rows=testPrototype(html.match(/<script>([\s\S]*?)<\/script>/)[1]);
console.table(rows); if(rows.some(r=>r.status!=='PASS'))process.exitCode=1;
