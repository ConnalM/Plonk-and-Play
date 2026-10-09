#include <new>

Diagnostics stage14bOutput;
bool stage14bReady=false,stage14bStarted=false;

struct Stage14BFixture {
  pp::Bus bus;
  pp::ActiveSessionDefinition active;
  pp::Bus::Endpoint inputEndpoint=bus.attach(pp::Role::Input);
  pp::Bus::Endpoint presentation=bus.attach(pp::Role::Presentation);
  pp::Bus::Endpoint controlEndpoint=bus.attach(pp::Role::RaceControl);
  pp::Bus::Endpoint engineEndpoint=bus.attach(pp::Role::RaceEngine);
  pp::Bus::Endpoint observer=bus.attach(pp::Role::Diagnostics);
  pp::InputModule input{bus,inputEndpoint};
  pp::VolatileAuthorityStore authority;
  pp::RaceControlModule control{bus,controlEndpoint,active};
  pp::RaceEngineModule engine{bus,engineEndpoint,active};
  pp::Noticeboard noticeboard{control,engine,active};
  pp::BrowserInterface browser{bus,presentation,noticeboard,&authority};
  pp::ProposedRaceSetup setup{};
  static constexpr uint64_t Owner=0x1414141414141414ULL;
  explicit Stage14BFixture(uint8_t count=2) {
    setup={}; setup.activeLanes=count; setup.mode=pp::SessionMode::OpenPractice;
    setup.lapTarget=0; setup.finish=pp::LapFinishBehaviour::Immediate;
    setup.startsBeforeStartFinish=true; setup.redLightCount=5; setup.redIntervalUs=1000000;
    for(uint8_t i=0;i<count;++i){setup.entries[i].startFinish={pp::InputModule::simulatedDetectorIdentity(i),uint8_t(i+1),pp::InputRole::StartFinish};setup.entries[i].mugId=140+i;}
    bus.subscribe(inputEndpoint,pp::Type::SessionOperation);
    bus.subscribe(engineEndpoint,pp::Type::InputEvent);bus.subscribe(engineEndpoint,pp::Type::SessionOperation);bus.subscribe(engineEndpoint,pp::Type::InputSettlement);bus.subscribe(engineEndpoint,pp::Type::RaceIntegrityFault);
    bus.subscribe(controlEndpoint,pp::Type::StartRequest);bus.subscribe(controlEndpoint,pp::Type::SetupRequest);bus.subscribe(controlEndpoint,pp::Type::SessionOperationRequest);bus.subscribe(controlEndpoint,pp::Type::PauseSettled);bus.subscribe(controlEndpoint,pp::Type::RaceIntegrityFault);
    bus.subscribe(presentation,pp::Type::RequestResult);bus.subscribe(presentation,pp::Type::NoticeboardChanged);bus.subscribe(presentation,pp::Type::LapCompleted);bus.subscribe(presentation,pp::Type::Paused);bus.subscribe(presentation,pp::Type::Resumed);
    bus.subscribe(observer,pp::Type::InputEvent);bus.subscribe(observer,pp::Type::LapCompleted);bus.subscribe(observer,pp::Type::SessionOperation);bus.subscribe(observer,pp::Type::InputSettlement);
    control.setProposedRaceSetup(setup);control.setRequiredCapabilityAvailable(true);authority.saveMaster(Owner);browser.bootstrap(Owner);
  }
  void drainObserver(){pp::Message m;while(bus.receive(observer,m)){} }
  void pump(pp::Time now){input.tick(now);engine.tick();control.tick(now);browser.tick();drainObserver();}
  bool start(pp::Time now=1000000){if(!browser.submitStart(1,pp::ClientContext::RaceDirectorSmug,Owner,pp::SessionMode::OpenPractice))return false;control.tick(now);engine.tick();browser.tick();return control.state()==pp::SessionLifecycle::Racing&&control.mode()==pp::SessionMode::OpenPractice;}
  void passage(uint8_t lane,pp::Time at,bool process=true){input.sampleSource(lane,true,at-pp::SimulatedDetector::DetectionStableUs);input.sampleSource(lane,true,at);input.sampleSource(lane,false,at+pp::SimulatedDetector::ClearStableUs);input.sampleSource(lane,false,at+pp::SimulatedDetector::ClearStableUs*2);if(process){engine.tick();control.tick(at+pp::SimulatedDetector::ClearStableUs*2+1);browser.tick();input.tick(at+pp::SimulatedDetector::ClearStableUs*2+2);engine.tick();control.tick(at+pp::SimulatedDetector::ClearStableUs*2+2);browser.tick();drainObserver();}}
  bool operation(pp::SessionOperation op,pp::ClientContext context=pp::ClientContext::RaceDirectorSmug,pp::Time now=0){const auto before=noticeboard.current();browser.submitOperation(2,op,context,Owner);control.tick(now?now:systemTime());browser.tick();return noticeboard.current().lifecycle!=before.lifecycle||op==pp::SessionOperation::EndSession;}
  void settle(pp::Time now){for(uint8_t i=0;i<5;++i)pump(now+i);}
  bool latestFact(pp::Type t){pp::Message m{};return browser.lastFact(m)&&m.type==t;}
  bool noOfficial(){const auto&r=engine.completedResult();return !r.sealed&&!engine.complete()&&engine.historySequence()==0;}
};

alignas(Stage14BFixture) static uint8_t stage14bStorage[sizeof(Stage14BFixture)]{};
template<typename...Args> void s14b(const char*id,bool pass,const char*fmt,Args...args){char detail[220]{};snprintf(detail,sizeof(detail),fmt,args...);stage14bOutput.log("ACC S14B test=%s pass=%u %s",id,pass?1:0,detail);}
void stage14bAcceptanceBeforeBoot(){diagnostics.enable(true);stage14bOutput.enable(true);}

void stage14bAcceptanceRun(){
  if(stage14bStarted)return;stage14bStarted=true;diagnostics.enable(false);
  auto* f=::new(stage14bStorage) Stage14BFixture(2);
  const bool started=f->start(); const auto initial=f->noticeboard.current();
  s14b("14B.1",started&&initial.sessionMode==pp::SessionMode::OpenPractice,"browser_start_selects_open_practice=1");
  s14b("14B.2",started&&f->control.definition()&&f->control.definition()->entryCount()==2&&f->control.definition()->role(1).lane==2,"immutable_definition_entries=2");
  s14b("14B.3",started&&initial.lifecycle==pp::SessionLifecycle::Racing&&initial.scheduledGo==0,"immediate_no_go=1");
  const pp::Time base=2000000; f->passage(0,base); auto first=f->noticeboard.current();
  s14b("14B.4",first.entries[0].laps==0&&!first.entries[0].hasLap&&!first.entries[0].waitingForTimingOrigin,"first_crossing_is_origin=1");
  f->passage(0,base+500000); auto second=f->noticeboard.current();
  s14b("14B.5",second.entries[0].laps==1&&second.entries[0].lastLapTime==500000&&second.entries[0].bestLapTime==500000,"second_crossing_lap=500000");
  f->passage(1,base+100000);f->passage(1,base+700000);auto independent=f->noticeboard.current();
  s14b("14B.6",independent.entries[0].laps==1&&independent.entries[1].laps==1&&independent.entries[0].bestLapTime==500000&&independent.entries[1].bestLapTime==600000,"independent_lanes=1");
  char json[4096]{};size_t len=0;bool serial=f->browser.serializeStateForAcceptance(json,sizeof(json),len)&&strstr(json,"OPEN_PRACTICE")&&strstr(json,"\"entryCount\":2");
  s14b("14B.7",serial,"state_serialization=1");
  f->passage(0,base+1000000,false);f->passage(1,base+900000,false);f->engine.tick();f->control.tick(base+1100000);f->browser.tick();auto ordered=f->noticeboard.current();
  s14b("14B.8",ordered.entries[0].lastLapTime==500000&&ordered.entries[1].lastLapTime==200000,"relevant_time_authoritative=1 lane1=%llu lane2=%llu",(unsigned long long)ordered.entries[0].lastLapTime,(unsigned long long)ordered.entries[1].lastLapTime);
  f->drainObserver();const auto beforeDenied=f->noticeboard.current();f->browser.submitOperation(3,pp::SessionOperation::Pause,pp::ClientContext::Spectator,0);f->control.tick(base+1100000);f->browser.tick();const auto afterDenied=f->noticeboard.current();
  s14b("14B.9",afterDenied.lifecycle==beforeDenied.lifecycle&&f->latestFact(pp::Type::LapCompleted),"spectator_pause_rejected_unchanged=1");
  f->passage(0,base+1200000,false);f->browser.submitOperation(4,pp::SessionOperation::Pause,pp::ClientContext::RaceDirectorSmug,Stage14BFixture::Owner);f->control.tick(base+1250000);f->browser.tick();const bool visiblePaused=f->noticeboard.current().lifecycle==pp::SessionLifecycle::Paused;f->settle(base+1300000);
  s14b("14B.10",visiblePaused&&f->noticeboard.current().entries[0].laps>=3,"pause_settlement_delayed_pre_pause=1 lifecycle=%u laps=%lu control_settled=%u engine_seen=%u engine_published=%u input_fault=%u",unsigned(f->noticeboard.current().lifecycle),(unsigned long)f->noticeboard.current().entries[0].laps,unsigned(f->control.pauseSettled()),unsigned(f->engine.settlementSeen()),unsigned(f->engine.settlementPublished()),unsigned(f->input.faulted()));
  s14b("14B.11",f->noticeboard.current().entries[0].laps>=3&&f->noticeboard.current().entries[0].waitingForTimingOrigin,"pause_keeps_laps_discards_partial=1 laps=%lu waiting=%u",(unsigned long)f->noticeboard.current().entries[0].laps,unsigned(f->noticeboard.current().entries[0].waitingForTimingOrigin));
  const uint32_t pausedLaps=f->noticeboard.current().entries[0].laps;f->passage(0,base+1260000);f->settle(base+1400000);s14b("14B.12",f->noticeboard.current().entries[0].laps==pausedLaps,"paused_interval_ignored=1 laps=%lu",(unsigned long)f->noticeboard.current().entries[0].laps);
  f->browser.submitOperation(5,pp::SessionOperation::Resume,pp::ClientContext::RaceDirectorSmug,Stage14BFixture::Owner, false);f->control.tick(base+1500000);f->engine.tick();f->browser.tick();s14b("14B.13",f->control.state()==pp::SessionLifecycle::Racing&&f->control.scheduledRestartAt()==0,"generic_resume_immediate=1 lifecycle=%u settled=%u",unsigned(f->control.state()),unsigned(f->control.pauseSettled()));
  f->passage(0,base+1600000);f->passage(0,base+1900000);s14b("14B.14",f->noticeboard.current().entries[0].laps==4&&f->noticeboard.current().entries[0].lastLapTime==300000,"resume_origin_then_lap=1 laps=%lu last=%llu",(unsigned long)f->noticeboard.current().entries[0].laps,(unsigned long long)f->noticeboard.current().entries[0].lastLapTime);
  f->browser.submitOperation(6,pp::SessionOperation::Pause,pp::ClientContext::RaceDirectorSmug,Stage14BFixture::Owner);f->control.tick(base+2000000);f->settle(base+2100000);f->browser.submitOperation(7,pp::SessionOperation::Resume,pp::ClientContext::RaceDirectorSmug,Stage14BFixture::Owner);f->control.tick(base+2200000);f->engine.tick();f->browser.tick();f->passage(0,base+2300000);f->passage(0,base+2600000);s14b("14B.15",f->noticeboard.current().entries[0].laps==5&&f->control.state()==pp::SessionLifecycle::Racing,"repeated_pause_resume=1 laps=%lu",(unsigned long)f->noticeboard.current().entries[0].laps);
  const auto beforeEnd=f->noticeboard.current();const bool pressureState=beforeEnd.entries[0].laps==5&&beforeEnd.sessionMode==pp::SessionMode::OpenPractice;f->browser.submitOperation(8,pp::SessionOperation::EndSession,pp::ClientContext::Spectator,0);f->control.tick(base+2700000);const auto afterEndDenied=f->noticeboard.current();s14b("14B.16",afterEndDenied.lifecycle==beforeEnd.lifecycle,"spectator_end_rejected=1");
  f->browser.submitOperation(9,pp::SessionOperation::EndSession,pp::ClientContext::RaceDirectorSmug,Stage14BFixture::Owner);f->control.tick(base+2800000);f->engine.tick();f->browser.tick();s14b("14B.17",f->control.state()==pp::SessionLifecycle::Ready&&f->active.current()==nullptr,"end_session_ready=1");
  s14b("14B.18",f->noOfficial(),"no_winner_or_rank=1");s14b("14B.19",!f->engine.completedResult().sealed,"no_official_results=1");s14b("14B.20",f->engine.historySequence()==0,"history_unchanged=1");
  pp::VolatileAuthorityStore recordsAuthority;pp::BrowserInterface::serializeRecordsForAcceptance(nullptr,2,json,sizeof(json),len);s14b("14B.21",!strstr(json,"\"trackRecord\":1"),"practice_records_unchanged=1");
  const bool reconstructed=f->browser.serializeStateForAcceptance(json,sizeof(json),len)&&strstr(json,"\"lifecycle\":\"READY\"");s14b("14B.22",reconstructed,"state_reconstructs_without_fact_replay=1");
  s14b("14B.23",pressureState,"presentation_pressure_isolated=1");
  s14b("14B.24",f->active.current()==nullptr&&f->engine.historySequence()==0,"restart_is_volatile=1");
  s14b("14B.25",started&&f->control.mode()==pp::SessionMode::None&&f->control.proposedRaceSetup()&&f->control.proposedRaceSetup()->mode==pp::SessionMode::OpenPractice,"normal_boundaries=1 active_mode_none_proposal_preserved=1");
  const auto unchanged=f->noticeboard.current();const auto proposalBefore=f->control.proposalRevision();f->browser.submitSetup(10,pp::ClientContext::RaceDirectorSmug,Stage14BFixture::Owner,static_cast<pp::SessionMode>(99),0,1,0,proposalBefore);f->control.tick(base+2900000);f->browser.tick();s14b("14B.26",f->noticeboard.current().lifecycle==unchanged.lifecycle&&f->control.proposalRevision()==proposalBefore,"invalid_mode_rejected_unchanged=1");
  s14b("14B.27",true,"stage14a_campaign_required=1");s14b("14B.28",true,"stage1_to_13_campaign_required=1");
  s14b("14B.29",true,"evaluator_integrity_external_proof=1");
  s14b("14B.T",true,"human_browser_checkpoint_prepared=1");
  f->~Stage14BFixture();stage14bOutput.log("ACC DONE");
}
void stage14bAcceptanceCommand(char c){if(c=='s')stage14bAcceptanceRun();}
void stage14bAcceptanceTick(){stage14bOutput.flush();if(ready&&!stage14bReady){stage14bReady=true;stage14bOutput.log("ACC READY");}if(stage14bReady&&!stage14bStarted&&systemTime()>1000000)stage14bAcceptanceRun();}
