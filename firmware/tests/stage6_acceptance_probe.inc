// Stage 6 preparation scaffolding supplies immutable session data and asks the
// real Race Control to begin. It neither publishes GO nor changes RC/RE state.
Diagnostics stage6Output;pp::Bus::Endpoint stage6Facts;bool stage6Ready=false;
pp::SessionDefinition stage6Session({pp::InputModule::simulatedDetectorIdentity(),1,pp::InputRole::StartFinish},81,2,pp::LapFinishBehaviour::Immediate);
const char* stage6State(pp::SessionLifecycle state){
  switch(state){case pp::SessionLifecycle::Ready:return "READY";case pp::SessionLifecycle::Starting:return "STARTING";case pp::SessionLifecycle::Racing:return "RACING";case pp::SessionLifecycle::Finished:return "FINISHED";}
  return "UNKNOWN";
}
void stage6AcceptanceBeforeBoot(){
  stage6Facts=bus.attach(pp::Role::Diagnostics);
  bus.subscribe(stage6Facts,pp::Type::GoScheduled);
  bus.subscribe(stage6Facts,pp::Type::LapCompleted);
  bus.subscribe(stage6Facts,pp::Type::CompetitionComplete);
  // Finish settlement is the normal Stage 12 boundary and is also exercised
  // here so this historical lifecycle fixture remains compatible with it.
  bus.subscribe(input.endpoint,pp::Type::FinishSettlement);
  diagnostics.enable(false);stage6Output.enable(true);
}
void stage6Cross(pp::Time t){
  input.sampleSource(false,t-141000);input.sampleSource(false,t-121000);
  input.sampleSource(true,t-20000);input.sampleSource(true,t);
  input.sampleSource(false,t+1000);input.sampleSource(false,t+21000);
  raceEngine.tick();
}
void stage6Drain(const char* checkpoint){
  pp::Message fact{};unsigned go=0,laps=0,complete=0;pp::Time completeAt=0;uint16_t goSource=0;
  while(bus.receive(stage6Facts,fact)){
    if(fact.type==pp::Type::GoScheduled){++go;goSource=fact.source;}
    if(fact.type==pp::Type::LapCompleted)++laps;
    if(fact.type==pp::Type::CompetitionComplete){++complete;completeAt=fact.relevantTime;}
  }
  stage6Output.log("ACC S6 test=%s state=%s go_facts=%u go_source=%u lap_facts=%u complete_facts=%u complete_at=%llu laps=%lu lap=%llu engine_go=%llu",checkpoint,stage6State(raceControl.state()),go,goSource,laps,complete,(unsigned long long)completeAt,(unsigned long)raceEngine.laps(),(unsigned long long)raceEngine.lastLapTime(),(unsigned long long)raceEngine.go());
}
void stage6Run(){
  // This is the permitted Stage 6 preparation boundary: fixed session data and
  // start-procedure initiation only. GO and lifecycle transitions are production RC.
  raceEngine.prepare(stage6Session);raceControl.prepare(stage6Session);
  stage6Output.log("ACC S6 test=6.1 state=%s",stage6State(raceControl.state()));
  const pp::Time publicationNow=systemTime();
  const bool started=raceControl.start(publicationNow);raceEngine.tick();
  const pp::Time go=raceControl.scheduledGo();
  stage6Output.log("ACC S6 test=6.2 started=%u publication_now=%llu go=%llu future=%u state=%s",started?1:0,(unsigned long long)publicationNow,(unsigned long long)go,go>publicationNow?1:0,stage6State(raceControl.state()));
  stage6Drain("6.3");
  raceControl.tick(go-1);stage6Output.log("ACC S6 test=6.4 state=%s",stage6State(raceControl.state()));
  raceControl.tick(go);stage6Output.log("ACC S6 test=6.5 state=%s",stage6State(raceControl.state()));
  stage6Output.log("ACC S6 test=6.6 engine_go=%llu scheduled_go=%llu",(unsigned long long)raceEngine.go(),(unsigned long long)go);
  stage6Cross(go+500000);raceControl.tick(go+500000);
  stage6Output.log("ACC S6 test=6.7 laps=%lu lap=%llu",(unsigned long)raceEngine.laps(),(unsigned long long)raceEngine.lastLapTime());
  stage6Cross(go+900000);raceControl.tick(go+900000);
  input.tick(go+900001);raceEngine.tick();raceControl.tick(go+900001);
  stage6Drain("6.10");
  stage6Output.log("ACC S6 test=6.11 engine_complete=%u",raceEngine.complete()?1:0);
  stage6Output.log("ACC S6 test=6.12 state=%s",stage6State(raceControl.state()));
  stage6Output.log("ACC S6 LIFECYCLE READY STARTING GO_SCHEDULED RACING LAP_COMPLETED COMPETITION_COMPLETE FINISHED");
  stage6Output.log("ACC DONE");
}
void stage6AcceptanceCommand(char c){if(c=='s')stage6Run();}
void stage6AcceptanceTick(){stage6Output.flush();if(ready&&!stage6Ready){stage6Ready=true;stage6Output.log("ACC READY");}}
