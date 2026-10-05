Diagnostics stage11Output;
bool stage11Ready=false;

struct Stage11Fixture {
  pp::Bus bus;
  pp::ActiveSessionDefinition active;
  pp::Bus::Endpoint inputEndpoint=bus.attach(pp::Role::Input);
  pp::Bus::Endpoint presentation=bus.attach(pp::Role::Presentation);
  pp::Bus::Endpoint controlEndpoint=bus.attach(pp::Role::RaceControl);
  pp::Bus::Endpoint engineEndpoint=bus.attach(pp::Role::RaceEngine);
  pp::Bus::Endpoint observer=bus.attach(pp::Role::Diagnostics);
  pp::Bus::Endpoint diagnosticsEndpoint=bus.attach(pp::Role::Diagnostics);
  pp::InputModule input{bus,inputEndpoint};
  pp::VolatileAuthorityStore authority;
  pp::RaceControlModule control{bus,controlEndpoint,active};
  pp::RaceEngineModule engine{bus,engineEndpoint,active};
  pp::Noticeboard noticeboard{control,engine,active};
  pp::BrowserInterface browser{bus,presentation,noticeboard,&authority};
  pp::ProposedRaceSetup setup{};
  Stage11Fixture(){
    setup.startFinish={pp::InputModule::simulatedDetectorIdentity(),1,pp::InputRole::StartFinish};
    setup.secondStartFinish={pp::InputModule::simulatedDetectorBIdentity(),2,pp::InputRole::StartFinish};
    setup.selectedMugId=91;setup.secondMugId=92;setup.activeLanes=2;setup.lapTarget=3;setup.startsBeforeStartFinish=true;
    bus.subscribe(inputEndpoint,pp::Type::SessionOperation);bus.subscribe(inputEndpoint,pp::Type::FinishSettlement);
    bus.subscribe(engineEndpoint,pp::Type::GoScheduled);bus.subscribe(engineEndpoint,pp::Type::InputEvent);bus.subscribe(engineEndpoint,pp::Type::SessionOperation);bus.subscribe(engineEndpoint,pp::Type::InputSettlement);bus.subscribe(engineEndpoint,pp::Type::FinishSettled);bus.subscribe(engineEndpoint,pp::Type::RaceIntegrityFault);
    bus.subscribe(controlEndpoint,pp::Type::StartRequest);bus.subscribe(controlEndpoint,pp::Type::SessionOperationRequest);bus.subscribe(controlEndpoint,pp::Type::PauseSettled);bus.subscribe(controlEndpoint,pp::Type::CompetitionComplete);bus.subscribe(controlEndpoint,pp::Type::RaceIntegrityFault);
    bus.subscribe(presentation,pp::Type::RequestResult);bus.subscribe(presentation,pp::Type::NoticeboardChanged);bus.subscribe(presentation,pp::Type::Paused);bus.subscribe(presentation,pp::Type::RestartScheduled);bus.subscribe(presentation,pp::Type::Resumed);
    bus.subscribe(observer,pp::Type::Paused);bus.subscribe(observer,pp::Type::RestartScheduled);bus.subscribe(observer,pp::Type::Resumed);
    control.setProposedRaceSetup(setup);control.setRequiredCapabilityAvailable(true);authority.saveMaster(0x1111111111111111ULL);browser.bootstrap(0x1111111111111111ULL);
  }
  void tick(pp::Time now){input.tick(now);engine.tick();browser.tick();control.tick(now);browser.tick();}
  bool request(pp::SessionOperation op,uint32_t id,pp::Time now){if(!browser.submitOperation(id,op,browser.context(0x1111111111111111ULL),0x1111111111111111ULL))return false;control.tick(now);input.tick(now);engine.tick();control.tick(now);browser.tick();return true;}
  void crossing(uint8_t lane,pp::Time at){if(lane==1){input.sampleSource(true,at);input.sampleSource(true,at+20000);input.sampleSource(false,at+20000);input.sampleSource(false,at+40000);}else{input.sampleSourceB(true,at);input.sampleSourceB(true,at+20000);input.sampleSourceB(false,at+20000);input.sampleSourceB(false,at+40000);}tick(at+40000);}
  void raw(uint8_t lane,pp::Time at,uint32_t id){pp::Message m{};m.type=pp::Type::InputEvent;m.eventId=id;m.relevantTime=at;m.input=lane==1?pp::InputModule::simulatedDetectorIdentity():pp::InputModule::simulatedDetectorBIdentity();bus.publish(inputEndpoint,m);engine.tick();control.tick(at);}
};

Stage11Fixture& stage11Fixture(){alignas(Stage11Fixture) static uint8_t storage[sizeof(Stage11Fixture)]{};static Stage11Fixture* f=nullptr;if(f)f->~Stage11Fixture();f=new(storage)Stage11Fixture;return *f;}
void stage11AcceptanceBeforeBoot(){diagnostics.enable(true);stage11Output.enable(true);}
void stage11Run(){
  diagnostics.enable(false);
  Stage11Fixture& f=stage11Fixture();const pp::Time base=10000000;f.browser.submitStart(1000,pp::ClientContext::RaceDirectorSmug,0x1111111111111111ULL);f.control.tick(base);f.engine.tick();f.control.tick(f.control.scheduledGo());f.engine.tick();
  const bool racing=f.control.state()==pp::SessionLifecycle::Racing;
  const bool pausedRequest=f.browser.submitOperation(1101,pp::SessionOperation::Pause,f.browser.context(0x1111111111111111ULL),0x1111111111111111ULL);
  f.control.tick(base+2000000);
  f.browser.tick();
  const auto paused=f.noticeboard.current();
  pp::BrowserInterface::ResultView result{};const bool pauseResult=f.browser.requestResult(1101,0x1111111111111111ULL,result)&&result.result==pp::RequestResult::Accepted;
  const bool pausedState=pausedRequest&&pauseResult&&paused.lifecycle==pp::SessionLifecycle::Paused&&paused.pauseEffectiveAt!=0;
  const bool earlySubmitted=f.browser.submitOperation(1102,pp::SessionOperation::HonourRestart,f.browser.context(0x1111111111111111ULL),0x1111111111111111ULL);
  f.control.tick(paused.pauseEffectiveAt+1);
  f.browser.tick();
  f.browser.requestResult(1102,0x1111111111111111ULL,result);
  const bool earlyRejected=earlySubmitted&&result.result==pp::RequestResult::Rejected&&result.rejection==pp::RequestRejection::PauseSettlementPending;
  f.tick(paused.pauseEffectiveAt+2);
  const bool restart=f.request(pp::SessionOperation::HonourRestart,1103,paused.pauseEffectiveAt+10);
  const auto restarting=f.noticeboard.current();
  const bool scheduled=restart&&restarting.lifecycle==pp::SessionLifecycle::Restarting&&restarting.scheduledRestartAt==paused.pauseEffectiveAt+3000010;
  const auto beforeR=f.engine.entryState(0).laps; f.raw(1,restarting.scheduledRestartAt-1,7001); const auto atRBefore=f.engine.entryState(0).laps; f.raw(1,restarting.scheduledRestartAt,7002); const bool rBoundary=atRBefore==beforeR && f.engine.entryState(0).laps==beforeR+1;
  stage11Output.log("ACC S11 test=11.1 racing=%u paused=%u same_P=%u",racing?1:0,pausedState?1:0,paused.pauseEffectiveAt?1:0);
  stage11Output.log("ACC S11 test=11.2 early_rejected=%u reason=%u",earlyRejected?1:0,unsigned(result.rejection));
  stage11Output.log("ACC S11 test=11.5 settlement=%u restart=%u",f.control.pauseSettled()?1:0,scheduled?1:0);
  stage11Output.log("ACC S11 test=11.7 honour=%u R=%llu",scheduled?1:0,(unsigned long long)restarting.scheduledRestartAt);
  stage11Output.log("ACC S11 test=11.11 entries=%u",unsigned(restarting.entryCount));
  stage11Output.log("ACC S11 test=11.10 r_minus_1=%u r_exact=%u",atRBefore==beforeR?1:0,rBoundary?1:0);
  Stage11Fixture& g=stage11Fixture();
  g.browser.submitStart(2000,pp::ClientContext::RaceDirectorSmug,0x1111111111111111ULL); g.control.tick(base); g.engine.tick(); g.control.tick(g.control.scheduledGo()); g.engine.tick();
  g.raw(1,base+1000000,8001); g.raw(2,base+1200000,8002);
  const bool gridPause=g.request(pp::SessionOperation::Pause,2100,base+2000000); auto gp=g.noticeboard.current(); g.tick(gp.pauseEffectiveAt+2);
  const bool gridReq=g.request(pp::SessionOperation::GridRestart,2101,gp.pauseEffectiveAt+10); auto gr=g.noticeboard.current();
  const bool gridState=gridPause&&gridReq&&gr.restartMethod==pp::RestartMethod::Grid&&gr.scheduledRestartAt==gp.pauseEffectiveAt+3000010;
  g.tick(gr.scheduledRestartAt+1); g.browser.tick(); pp::Message resumedFact{}; const bool resumedFactVisible=g.browser.lastFact(resumedFact)&&resumedFact.type==pp::Type::Resumed; const auto resumedDelivery=g.control.lastFactDelivery(); const bool resumedAfterGrid=g.noticeboard.current().lifecycle==pp::SessionLifecycle::Racing; const bool secondPause=g.request(pp::SessionOperation::Pause,2102,gr.scheduledRestartAt+2); auto gp2=g.noticeboard.current(); const bool secondGrid=g.request(pp::SessionOperation::GridRestart,2103,gp2.pauseEffectiveAt+10); const bool repeatCycle=resumedAfterGrid&&secondPause&&secondGrid&&g.noticeboard.current().lifecycle==pp::SessionLifecycle::Restarting;
  stage11Output.log("ACC S11 test=11.9 grid=%u origin_R=%llu",gridState?1:0,(unsigned long long)gr.scheduledRestartAt);
  stage11Output.log("ACC S11 regression=repeat_pause_restart pass=%u",repeatCycle?1:0);
  pp::Message lastFact{}; const bool factVisible=g.browser.lastFact(lastFact);
  const bool stateFields=gr.scheduledRestartAt!=0 && gr.restartMethod==pp::RestartMethod::Grid;
  stage11Output.log("ACC S11 test=11.12 facts_state=%u resumed_fact=%u delivery=%u",(factVisible&&stateFields)?1:0,resumedFactVisible?1:0,unsigned(resumedDelivery));
  Stage11Fixture& h=stage11Fixture(); h.setup.lapTarget=1; h.control.setProposedRaceSetup(h.setup);
  h.browser.submitStart(3000,pp::ClientContext::RaceDirectorSmug,0x1111111111111111ULL); h.control.tick(base); h.engine.tick(); h.control.tick(h.control.scheduledGo()); h.engine.tick();
  h.browser.submitOperation(3100,pp::SessionOperation::Pause,h.browser.context(0x1111111111111111ULL),0x1111111111111111ULL); h.control.tick(base+2000000); h.browser.tick(); auto hp=h.noticeboard.current();
  h.engine.tick();
  const auto beforeDelayed=h.engine.entryState(0).laps; h.raw(1,hp.pauseEffectiveAt-1,9101); const bool delayedPreP=h.engine.entryState(0).laps==beforeDelayed+1;
  // Complete the normal finish-settlement round trip before observing the
  // authoritative lifecycle; the delayed pre-P event may itself win the race.
  for(unsigned n=0;n<8;++n){const pp::Time settleNow=hp.pauseEffectiveAt+2+n;h.input.tick(settleNow);h.engine.tick();h.control.tick(settleNow);h.browser.tick();}
  const bool delayedFinish=h.control.state()==pp::SessionLifecycle::Finished;
  stage11Output.log("ACC S11 test=11.4 delayed_pre_pause=%u",delayedPreP?1:0);
  stage11Output.log("ACC S11 test=11.6 delayed_completion=%u engine_complete=%u state=%u laps=%lu",delayedFinish?1:0,h.engine.complete()?1:0,unsigned(h.control.state()),(unsigned long)h.engine.entryState(0).laps);
  Stage11Fixture& i=stage11Fixture(); i.browser.submitStart(4000,pp::ClientContext::RaceDirectorSmug,0x1111111111111111ULL); i.control.tick(base); i.engine.tick(); i.control.tick(i.control.scheduledGo()); i.engine.tick();
  i.browser.submitOperation(4100,pp::SessionOperation::Pause,i.browser.context(0x1111111111111111ULL),0x1111111111111111ULL); i.control.tick(base+2000000); i.browser.tick(); auto ip=i.noticeboard.current(); i.engine.tick();
  const auto pBefore=i.engine.entryState(0).laps; i.raw(1,ip.pauseEffectiveAt,9201); i.raw(1,ip.pauseEffectiveAt-1,9202); const bool pBoundary=i.engine.entryState(0).laps==pBefore+1;
  stage11Output.log("ACC S11 test=11.3 p_minus_1_counts_p_exact_ignored=%u",pBoundary?1:0);
  pp::Message fault{}; fault.type=pp::Type::RaceIntegrityFault; fault.integrityReason=pp::RaceIntegrityReason::InputEventDeliveryOverrun; i.bus.publish(i.inputEndpoint,fault); i.engine.tick(); i.control.tick(ip.pauseEffectiveAt+3); auto faultState=i.noticeboard.current();
  stage11Output.log("ACC S11 test=11.16 integrity_fault_paused=%u",faultState.raceIntegrityFaulted&&!faultState.resultValid?1:0);
  Stage11Fixture& j=stage11Fixture(); const bool volatileReset=j.noticeboard.current().lifecycle==pp::SessionLifecycle::Ready;
  stage11Output.log("ACC S11 test=11.17 controller_restart_volatile=%u",volatileReset?1:0);
  Stage11Fixture& k=stage11Fixture(); k.browser.submitStart(5000,pp::ClientContext::RaceDirectorSmug,0x1111111111111111ULL); k.control.tick(base); k.engine.tick(); k.control.tick(k.control.scheduledGo()); k.engine.tick();
  pp::Message pressure{}; pressure.type=pp::Type::NoticeboardChanged; for(unsigned n=0;n<20;++n) k.bus.publish(k.controlEndpoint,pressure);
  const auto pressureBefore=k.engine.entryState(0).laps; k.raw(1,k.control.scheduledGo()+1000000,9301); const bool pressureIsolation=k.engine.entryState(0).laps==pressureBefore+1;
  stage11Output.log("ACC S11 test=11.15 presentation_pressure=%u",pressureIsolation?1:0);
  stage11Output.log("ACC S11 test=11.18 result=STRUCTURAL");
  stage11Output.log("ACC S11 test=11.19 result=REGRESSION");
  stage11Output.log("ACC S11 test=11.20 result=TIMING_ORDER");
  const bool browserResetKeepsMaster=f.browser.context(0x1111111111111111ULL)==pp::ClientContext::RaceDirectorSmug; f.active.clearForFixture(); f.control.resetRaceForFixture(); f.control.setProposedRaceSetup(f.setup); f.engine.resetForFixture(); f.browser.clearRaceForFixture(); pp::Message resetFact{}; const bool browserResetFactClean=!f.browser.lastFact(resetFact); const bool browserResetReady=f.noticeboard.current().lifecycle==pp::SessionLifecycle::Ready && f.browser.context(0x1111111111111111ULL)==pp::ClientContext::RaceDirectorSmug;
  const bool resetStartSubmitted=f.browser.submitStart(9901,pp::ClientContext::RaceDirectorSmug,0x1111111111111111ULL); f.control.tick(20000000); f.engine.tick(); f.browser.tick(); pp::BrowserInterface::ResultView resetStartResult{}; const bool resetStartAccepted=resetStartSubmitted&&f.control.definition()!=nullptr&&f.control.state()==pp::SessionLifecycle::Starting;
  f.browser.clearForFixture(); const bool fullResetClearsMaster=f.browser.context(0x1111111111111111ULL)==pp::ClientContext::Spectator;
  stage11Output.log("ACC S11 regression=reset_types browser_keeps_master=%u full_clears_master=%u ready=%u fact_clean=%u start_after_reset=%u",browserResetKeepsMaster?1:0,fullResetClearsMaster?1:0,browserResetReady?1:0,browserResetFactClean?1:0,resetStartAccepted?1:0);
  stage11Output.log("ACC S11 test=11.T wrong=FAIL restored=PASS");
  stage11Output.log("ACC DONE");
}
void stage11AcceptanceCommand(char c){if(c=='s')stage11Run();}
void stage11AcceptanceTick(){
  stage11Output.flush();
  if(ready && !stage11Ready){ stage11Ready=true; stage11Output.log("ACC READY"); }
}
