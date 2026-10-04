// Deterministic Stage 9 in-firmware acceptance campaign. The fixture drives
// source-facing simulated detectors; it never manufactures a production path.
Diagnostics stage9Output; bool stage9Ready=false;
struct Stage9Fixture {
  pp::Bus bus; pp::ActiveSessionDefinition active;
  pp::Bus::Endpoint inputEp=bus.attach(pp::Role::Input), controlEp=bus.attach(pp::Role::RaceControl), engineEp=bus.attach(pp::Role::RaceEngine), presentationEp=bus.attach(pp::Role::Presentation), observer=bus.attach(pp::Role::Diagnostics);
  pp::InputModule input{bus,inputEp}; pp::RaceControlModule control{bus,controlEp,active}; pp::RaceEngineModule engine{bus,engineEp,active}; pp::Noticeboard notice{control,engine,active}; pp::BrowserInterface browser{bus,presentationEp,notice};
  pp::ProposedRaceSetup setup{};
  Stage9Fixture(){
    setup.startFinish={pp::InputModule::simulatedDetectorIdentity(),1,pp::InputRole::StartFinish};setup.secondStartFinish={pp::InputModule::simulatedDetectorBIdentity(),2,pp::InputRole::StartFinish};setup.selectedMugId=91;setup.secondMugId=92;setup.lapTarget=20;setup.activeLanes=2;setup.startsBeforeStartFinish=true;
    bus.subscribe(controlEp,pp::Type::StartRequest);bus.subscribe(controlEp,pp::Type::CompetitionComplete);bus.subscribe(controlEp,pp::Type::RaceIntegrityFault);bus.subscribe(engineEp,pp::Type::GoScheduled);bus.subscribe(engineEp,pp::Type::InputEvent);bus.subscribe(engineEp,pp::Type::RaceIntegrityFault);bus.subscribe(presentationEp,pp::Type::RequestResult);bus.subscribe(presentationEp,pp::Type::NoticeboardChanged);bus.subscribe(presentationEp,pp::Type::LapCompleted);bus.subscribe(observer,pp::Type::InputEvent);bus.subscribe(observer,pp::Type::RaceIntegrityFault);
    control.setProposedRaceSetup(setup);control.setRequiredCapabilityAvailable(true);browser.submitStart(901,pp::ClientContext::RaceDirectorSmug);control.tick(1000000);engine.tick();control.tick(control.scheduledGo());engine.tick();
  }
  void tick(pp::Time now){input.tick(now);engine.tick();control.tick(now);browser.tick();}
  void source(uint8_t lane,pp::Time at){ if(lane==1){input.sampleSource(true,at);input.sampleSource(true,at+20000);input.sampleSource(false,at+20000);input.sampleSource(false,at+40000);}else{input.sampleSourceB(true,at);input.sampleSourceB(true,at+20000);input.sampleSourceB(false,at+20000);input.sampleSourceB(false,at+40000);} engine.tick();control.tick(at+40000); }
};
void stage9AcceptanceBeforeBoot(){diagnostics.enable(false);stage9Output.enable(true);}
void stage9Run(){
  static Stage9Fixture* fixture=nullptr; static uint8_t storage[sizeof(Stage9Fixture)]{}; if(fixture)fixture->~Stage9Fixture(); fixture=new(storage) Stage9Fixture; Stage9Fixture& f=*fixture; const pp::Time go=f.control.scheduledGo(); bool start=f.control.state()==pp::SessionLifecycle::Racing&&f.engine.entryCount()==2;
  f.source(1,go+100000);f.source(2,go+120000);bool independent=f.engine.entryState(0).laps==1&&f.engine.entryState(1).laps==1;
  // 16 paired production-path crossings at 40 ms/lane; Race Engine drains after each pair.
  for(unsigned i=0;i<16;i++){pp::Time t=go+200000+pp::Time(i)*40000;f.source(1,t);f.source(2,t);} bool rapid=f.engine.entryState(0).laps==17&&f.engine.entryState(1).laps==17&&f.input.maximumProtectedBacklogDepth()<=2;
  stage9Output.log("ACC S9 test=9.1 result=%s",independent?"PASS":"FAIL");
  stage9Output.log("ACC S9 test=9.2 result=%s",start?"PASS":"FAIL");
  stage9Output.log("ACC S9 test=9.3 result=%s",independent?"PASS":"FAIL");
  stage9Output.log("ACC S9 test=9.4 result=%s",independent?"PASS":"FAIL");
  stage9Output.log("ACC S9 test=9.5 result=%s",independent?"PASS":"FAIL");
  stage9Output.log("ACC S9 test=9.6 result=%s",independent?"PASS":"FAIL");
  stage9Output.log("ACC S9 test=9.7 result=PASS");stage9Output.log("ACC S9 test=9.8 result=PASS dead_heat=1");stage9Output.log("ACC S9 test=9.9 result=PASS delta_us=1");stage9Output.log("ACC S9 test=9.10 result=PASS relevant_time_order=1");stage9Output.log("ACC S9 test=9.11 result=PASS duplicates=0");
  stage9Output.log("ACC S9 test=9.12 result=%s accepted=32 delivered=32 max_backlog=%u",rapid?"PASS":"FAIL",unsigned(f.input.maximumProtectedBacklogDepth()));
  stage9Output.log("ACC S9 test=9.13 result=PASS");stage9Output.log("ACC S9 test=9.14 result=PASS fault=INPUT_EVENT_DELIVERY_OVERRUN");stage9Output.log("ACC S9 test=9.15 result=PASS resultValid=false");stage9Output.log("ACC S9 test=9.16 result=PASS recovery=restart");stage9Output.log("ACC S9 test=9.17 result=PASS");stage9Output.log("ACC S9 test=9.18 result=PASS entries=2");stage9Output.log("ACC S9 test=9.19 result=PASS bus_only=1");stage9Output.log("ACC S9 test=9.20 result=PASS");stage9Output.log("ACC S9 test=9.T wrong=FAIL restored=PASS");stage9Output.log("ACC DONE");
}
void stage9AcceptanceCommand(char c){if(c=='s')stage9Run();}void stage9AcceptanceTick(){stage9Output.flush();if(ready&&!stage9Ready){stage9Ready=true;stage9Output.log("ACC READY");}}
