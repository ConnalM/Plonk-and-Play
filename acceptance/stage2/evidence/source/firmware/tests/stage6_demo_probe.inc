// Human-review demonstration only. It drives the existing source-facing
// simulated detector and real Race Control boundary automatically, and keeps
// the serial s/c controls for an additional manual run.
Diagnostics stage6DemoOutput;pp::Bus::Endpoint stage6DemoFacts;
pp::SessionDefinition stage6DemoSession({pp::InputModule::simulatedDetectorIdentity(),1,pp::InputRole::StartFinish},91,2,pp::LapFinishBehaviour::Immediate);
bool stage6DemoAutoStarted=false;uint8_t stage6DemoAutoCrossings=0;
pp::Time stage6DemoNextCrossingAt=0,stage6DemoReleaseAt=0,stage6DemoRearmAt=0;pp::SessionLifecycle stage6DemoLastState=pp::SessionLifecycle::Finished;
const char* stage6DemoState(pp::SessionLifecycle state){
  switch(state){case pp::SessionLifecycle::Ready:return "READY";case pp::SessionLifecycle::Starting:return "STARTING";case pp::SessionLifecycle::Racing:return "RACING";case pp::SessionLifecycle::Finished:return "FINISHED";}
  return "UNKNOWN";
}
void stage6DemoBeforeBoot(){
  stage6DemoFacts=bus.attach(pp::Role::Diagnostics);
  bus.subscribe(stage6DemoFacts,pp::Type::GoScheduled);bus.subscribe(stage6DemoFacts,pp::Type::LapCompleted);bus.subscribe(stage6DemoFacts,pp::Type::CompetitionComplete);
  diagnostics.enable(false);stage6DemoOutput.enable(true);stage6DemoOutput.log("[DEMO] READY: automatic race begins shortly");
}
void stage6DemoStart(){
  raceEngine.prepare(stage6DemoSession);raceControl.prepare(stage6DemoSession);
  if(raceControl.start(systemTime())){raceEngine.tick();stage6DemoOutput.log("[DEMO] STARTING: Race Control scheduled GO at %llu us",(unsigned long long)raceControl.scheduledGo());}
}
void stage6DemoTick(){
  const pp::Time now=systemTime();
  if(!stage6DemoAutoStarted){stage6DemoAutoStarted=true;stage6DemoStart();}
  if(raceControl.state()==pp::SessionLifecycle::Racing&&!stage6DemoReleaseAt&&!stage6DemoRearmAt&&stage6DemoAutoCrossings<2&&now>=stage6DemoNextCrossingAt){input.setSimulatedSource(true);stage6DemoReleaseAt=now+25000;stage6DemoNextCrossingAt=now+250000;++stage6DemoAutoCrossings;stage6DemoOutput.log("[DEMO] detector ACTIVE: clean crossing will follow");}
  if(stage6DemoReleaseAt&&now>=stage6DemoReleaseAt){input.setSimulatedSource(false);stage6DemoReleaseAt=0;stage6DemoRearmAt=now+21000;}
  if(stage6DemoRearmAt&&now>=stage6DemoRearmAt){stage6DemoRearmAt=0;stage6DemoOutput.log("[DEMO] detector rearmed");}
  pp::Message fact;while(bus.receive(stage6DemoFacts,fact)){
    if(fact.type==pp::Type::GoScheduled)stage6DemoOutput.log("[DEMO] GO_SCHEDULED: %llu us",(unsigned long long)fact.relevantTime);
    else if(fact.type==pp::Type::LapCompleted)stage6DemoOutput.log("[DEMO] LAP_COMPLETED: lap=%lu lap_time=%llu us",(unsigned long)fact.lapNumber,(unsigned long long)fact.lapTime);
    else if(fact.type==pp::Type::CompetitionComplete)stage6DemoOutput.log("[DEMO] COMPETITION_COMPLETE at %llu us",(unsigned long long)fact.relevantTime);
  }
  if(raceControl.state()!=stage6DemoLastState){stage6DemoLastState=raceControl.state();stage6DemoOutput.log("[DEMO] %s",stage6DemoState(stage6DemoLastState));}
  stage6DemoOutput.flush();
}
void stage6DemoCommand(char c){if(c=='s')stage6DemoStart();else if(c=='c'&&raceControl.state()==pp::SessionLifecycle::Racing&&!stage6DemoReleaseAt&&!stage6DemoRearmAt){input.setSimulatedSource(true);stage6DemoReleaseAt=systemTime()+25000;stage6DemoOutput.log("[DEMO] detector ACTIVE: clean crossing will follow");}}
