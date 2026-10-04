// Human-review-only Stage 7 scaffolding. It repeatedly exercises the accepted
// one-lane fixed-lap path so a Browser can join and observe a complete race.
pp::SessionDefinition stage7DemoSession({pp::InputModule::simulatedDetectorIdentity(),1,pp::InputRole::StartFinish},91,2,pp::LapFinishBehaviour::Immediate);
pp::Time stage7DemoStartAt=0,stage7DemoNextCycle=0;bool stage7DemoPrepared=false,stage7DemoFirst=false,stage7DemoSecond=false;
void stage7DemoCross(pp::Time t){input.sampleSource(false,t-141000);input.sampleSource(false,t-121000);input.sampleSource(true,t-20000);input.sampleSource(true,t);input.sampleSource(false,t+1000);input.sampleSource(false,t+21000);raceEngine.tick();raceControl.tick(t);browser.tick();}
void stage7DemoPrepare(pp::Time now){raceEngine.prepare(stage7DemoSession);raceControl.prepare(stage7DemoSession);noticeboard.setSession(stage7DemoSession);browser.tick();stage7DemoStartAt=now+5000000;stage7DemoPrepared=true;stage7DemoFirst=false;stage7DemoSecond=false;}
void stage7DemoBeforeBoot(){stage7DemoNextCycle=systemTime();}
void stage7DemoTick(){
  const pp::Time now=systemTime();
  if(!stage7DemoPrepared&&now>=stage7DemoNextCycle)stage7DemoPrepare(now);
  if(stage7DemoPrepared&&raceControl.state()==pp::SessionLifecycle::Ready&&now>=stage7DemoStartAt){raceControl.start(now);raceEngine.tick();browser.tick();}
  const pp::Time go=raceControl.scheduledGo();
  if(raceControl.state()==pp::SessionLifecycle::Starting&&now>=go){raceControl.tick(go);browser.tick();}
  if(raceControl.state()==pp::SessionLifecycle::Racing&&!stage7DemoFirst&&now>=go+2000000){stage7DemoFirst=true;stage7DemoCross(go+2000000);}
  if(raceControl.state()==pp::SessionLifecycle::Racing&&!stage7DemoSecond&&now>=go+4000000){stage7DemoSecond=true;stage7DemoCross(go+4000000);}
  if(stage7DemoPrepared&&raceControl.state()==pp::SessionLifecycle::Finished){stage7DemoPrepared=false;stage7DemoNextCycle=now+10000000;}
}
void stage7DemoCommand(char){}
