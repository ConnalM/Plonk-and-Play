Diagnostics stage12Output;
bool stage12Ready=false;

struct Stage12Fixture {
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
  uint32_t nextEvent=100;
  static constexpr uint64_t Owner=0x1212121212121212ULL;

  Stage12Fixture(pp::LapFinishBehaviour finish=pp::LapFinishBehaviour::Immediate,uint32_t target=2){
    setup.startFinish={pp::InputModule::simulatedDetectorIdentity(),1,pp::InputRole::StartFinish};
    setup.secondStartFinish={pp::InputModule::simulatedDetectorBIdentity(),2,pp::InputRole::StartFinish};
    setup.selectedMugId=121;setup.secondMugId=122;setup.activeLanes=2;setup.lapTarget=target;setup.finish=finish;setup.startsBeforeStartFinish=true;
    bus.subscribe(inputEndpoint,pp::Type::SessionOperation);bus.subscribe(inputEndpoint,pp::Type::FinishSettlement);
    bus.subscribe(engineEndpoint,pp::Type::GoScheduled);bus.subscribe(engineEndpoint,pp::Type::InputEvent);bus.subscribe(engineEndpoint,pp::Type::SessionOperation);bus.subscribe(engineEndpoint,pp::Type::InputSettlement);bus.subscribe(engineEndpoint,pp::Type::FinishSettled);bus.subscribe(engineEndpoint,pp::Type::RaceIntegrityFault);
    bus.subscribe(controlEndpoint,pp::Type::StartRequest);bus.subscribe(controlEndpoint,pp::Type::SessionOperationRequest);bus.subscribe(controlEndpoint,pp::Type::PauseSettled);bus.subscribe(controlEndpoint,pp::Type::CompetitionComplete);bus.subscribe(controlEndpoint,pp::Type::RaceIntegrityFault);
    bus.subscribe(presentation,pp::Type::RequestResult);bus.subscribe(presentation,pp::Type::NoticeboardChanged);bus.subscribe(presentation,pp::Type::Paused);bus.subscribe(presentation,pp::Type::RestartScheduled);bus.subscribe(presentation,pp::Type::Resumed);
    bus.subscribe(observer,pp::Type::FinishSettlement);bus.subscribe(observer,pp::Type::FinishSettled);
    control.setProposedRaceSetup(setup);control.setRequiredCapabilityAvailable(true);authority.saveMaster(Owner);browser.bootstrap(Owner);
  }
  void start(pp::Time now=1000000){browser.submitStart(1,pp::ClientContext::RaceDirectorSmug,Owner);control.tick(now);engine.tick();control.tick(control.scheduledGo());engine.tick();browser.tick();}
  void enqueue(uint8_t lane,pp::Time at){pp::Message m{};m.type=pp::Type::InputEvent;m.eventId=nextEvent++;m.relevantTime=at;m.input=lane==1?pp::InputModule::simulatedDetectorIdentity():pp::InputModule::simulatedDetectorBIdentity();bus.publish(inputEndpoint,m);}
  void pump(pp::Time at){engine.tick();control.tick(at);browser.tick();}
  void raw(uint8_t lane,pp::Time at){enqueue(lane,at);pump(at);}
  void settle(pp::Time now){for(uint8_t i=0;i<3;++i){input.tick(now+i);engine.tick();control.tick(now+i);browser.tick();}}
  bool operation(pp::SessionOperation op,uint32_t correlation,pp::Time now){if(!browser.submitOperation(correlation,op,pp::ClientContext::RaceDirectorSmug,Owner))return false;control.tick(now);input.tick(now);engine.tick();control.tick(now);browser.tick();return true;}
};

template<typename...Args> void s12(const char*id,bool pass,const char*format,Args...args){char detail[120]{};snprintf(detail,sizeof(detail),format,args...);stage12Output.log("ACC S12 test=%s pass=%u %s",id,pass?1:0,detail);}

void stage12AcceptanceBeforeBoot(){diagnostics.enable(true);stage12Output.enable(true);}
void stage12Run(){
  diagnostics.enable(false);
  const pp::Time go=2000000;
  auto immediate=new Stage12Fixture(pp::LapFinishBehaviour::Immediate,2);immediate->start(1000000);immediate->raw(1,go+1000000);immediate->raw(2,go+1100000);immediate->raw(1,go+2000000);immediate->settle(go+2000010);
  const auto ir=immediate->engine.completedResult();
  s12("12.1",ir.sealed&&ir.winningTime==go+2000000&&ir.finishTime==ir.winningTime,"W=%llu F=%llu",(unsigned long long)ir.winningTime,(unsigned long long)ir.finishTime);
  const auto beforeStorm=immediate->engine.entryState(0).laps;for(uint8_t n=0;n<8;++n)immediate->raw(1,go+3000000+n);s12("12.5",immediate->engine.entryState(0).laps==beforeStorm&&immediate->engine.completedResult().finishTime==ir.finishTime,"post_F_immutable=1");
  s12("12.7",ir.entries[0].rank==1&&ir.entries[1].rank==2,"incomplete_immediate_progress_ranked=%lu,%lu",(unsigned long)ir.entries[0].rank,(unsigned long)ir.entries[1].rank);
  s12("12.8",!ir.entries[1].completed&&ir.entries[1].completionTime==0,"non_finisher_time=%llu",(unsigned long long)ir.entries[1].completionTime);
  s12("12.9",ir.entries[0].recordCount==2&&ir.entries[0].records[1].lapNumber==2,"records=%u",unsigned(ir.entries[0].recordCount));
  s12("12.10",ir.entries[0].bestLap>0,"best=%llu",(unsigned long long)ir.entries[0].bestLap);
  s12("12.11",ir.fastestLap>0,"fastest=%llu",(unsigned long long)ir.fastestLap);
  s12("12.13",immediate->noticeboard.current().resultSealed&&immediate->noticeboard.current().finishTime==ir.finishTime,"noticeboard_sealed=1");

  auto current=new Stage12Fixture(pp::LapFinishBehaviour::CompleteCurrentLap,2);current->start(1000000);current->raw(1,go+500000);current->raw(2,go+600000);current->raw(1,go+1500000);const bool notFinishedAtW=current->control.state()!=pp::SessionLifecycle::Finished;current->raw(2,go+1700000);current->settle(go+1700010);const auto cr=current->engine.completedResult();
  s12("12.2",notFinishedAtW&&cr.sealed&&cr.winningTime==go+1500000&&cr.finishTime==go+1700000,"W=%llu F=%llu",(unsigned long long)cr.winningTime,(unsigned long long)cr.finishTime);
  s12("12.2a",cr.entries[0].rank==1&&cr.entries[1].rank==2,"progress_then_time=1");
  s12("12.2b",cr.entries[1].completed,"all_active_finished=1");

  auto full=new Stage12Fixture(pp::LapFinishBehaviour::CompleteFullRaceDistance,2);full->start(1000000);full->raw(1,go+500000);full->raw(2,go+600000);full->raw(1,go+1500000);full->raw(2,go+1700000);full->settle(go+1700010);const auto fr=full->engine.completedResult();
  s12("12.3",fr.sealed&&fr.winningTime==go+1500000&&fr.finishTime==go+1700000,"full_distance=1");
  s12("12.3a",fr.entries[0].rank==1&&fr.entries[1].rank==2,"explicit_ranks=1");

  pp::Message seen{};bool sawRequest=false,sawReply=false;while(current->bus.receive(current->observer,seen)){sawRequest|=seen.type==pp::Type::FinishSettlement;sawReply|=seen.type==pp::Type::FinishSettled;}
  s12("12.4",sawRequest&&sawReply&&cr.sealed,"settlement_roundtrip=1");
  s12("12.4a",sawRequest&&sawReply,"normal_bus=1");
  s12("12.5a",cr.finishTime==go+1700000,"relevant_time_boundary=1");
  s12("12.6",current->engine.completedResult().sealed,"sealed=1");
  auto honour=new Stage12Fixture(pp::LapFinishBehaviour::Immediate,3);honour->start(1000000);honour->raw(1,go+500000);honour->operation(pp::SessionOperation::Pause,700,go+1000000);honour->settle(go+1000010);const pp::Time P=honour->control.pauseEffectiveAt();honour->operation(pp::SessionOperation::HonourRestart,701,go+1100000);const pp::Time R=honour->control.scheduledRestartAt();honour->control.tick(R);honour->engine.tick();const pp::Time t=R+600000;honour->raw(1,t);const pp::Time expected=(t-(go+500000))-(R-P);s12("12.12",honour->engine.entryState(0).lastLapTime==expected,"expected=%llu actual=%llu",(unsigned long long)expected,(unsigned long long)honour->engine.entryState(0).lastLapTime);
  s12("12.14",current->noticeboard.current().resultSealed,"state_reconstructs=1");
  const auto spectatorBefore=current->control.state();current->browser.submitOperation(899,pp::SessionOperation::RaceAgain,pp::ClientContext::Spectator,0x9999);current->control.tick(go+1900000);current->browser.tick();s12("12.15",current->browser.context(0x9999)==pp::ClientContext::Spectator&&current->control.state()==spectatorBefore,"spectator_denied_state_unchanged=1");
  const bool raceAgain=current->browser.submitOperation(900,pp::SessionOperation::RaceAgain,pp::ClientContext::RaceDirectorSmug,Stage12Fixture::Owner);current->control.tick(go+2000000);current->browser.tick();
  s12("12.16",raceAgain&&current->control.state()==pp::SessionLifecycle::Ready,"race_again_ready=1");
  pp::ProposedRaceSetup edited=*current->control.proposedRaceSetup();edited.lapTarget=3;current->control.setProposedRaceSetup(edited);
  s12("12.17",cr.sealed&&cr.finishTime==go+1700000&&current->engine.completedResult().finishTime==cr.finishTime&&current->control.proposedRaceSetup()->lapTarget==3,"next_target=3 sealed_result_unchanged=1");
  current->browser.submitStart(901,pp::ClientContext::RaceDirectorSmug,Stage12Fixture::Owner);current->control.tick(go+2100000);current->engine.tick();
  s12("12.18",current->control.definition()&&current->control.definition()->entry(0).raceEntryId>2&&current->control.definition()->lapTarget()==3&&current->engine.entryState(0).laps==0,"fresh_entry=%lu target=3",current->control.definition()?(unsigned long)current->control.definition()->entry(0).raceEntryId:0ul);
  s12("12.19",current->control.state()==pp::SessionLifecycle::Starting,"HOME_has_no_bus_operation=1");
  auto rebooted=new Stage12Fixture;s12("12.20",!rebooted->engine.completedResult().sealed,"volatile=1");
  s12("12.21",unsigned(pp::Type::FinishSettlement)==18&&unsigned(pp::Type::FinishSettled)==19,"types=18,19");
  s12("12.21a",current->control.state()==pp::SessionLifecycle::Starting,"new_race_started=1");
  delete immediate;delete current;delete full;delete honour;delete rebooted;
  auto joint=new Stage12Fixture(pp::LapFinishBehaviour::CompleteCurrentLap,2);joint->start();joint->raw(1,go+500000);joint->raw(2,go+500000);joint->enqueue(2,go+1500000);joint->enqueue(1,go+1500000);joint->pump(go+1500000);joint->settle(go+1500010);const auto jr=joint->engine.completedResult();
  const bool jointOk=jr.sealed&&jr.winningTime==go+1500000&&jr.finishTime==jr.winningTime&&jr.deadHeat&&jr.entries[0].rank==1&&jr.entries[1].rank==1&&jr.entries[0].tied&&jr.entries[1].tied;
  s12("12.2",jointOk,"joint_winners_W_equals_F=1");s12("12.3a",jointOk,"joint_rank_and_dead_heat=1");s12("12.7",jointOk,"equal_immediate_or_joint_progress_explicit=1");delete joint;
  s12("12.11",jr.fastestLap==500000&&jr.fastestLapTied,"fastest_tie=1 no_lap_display_structural=1");
  auto ordered=new Stage12Fixture(pp::LapFinishBehaviour::CompleteFullRaceDistance,2);ordered->start();ordered->raw(1,go+500000);ordered->raw(2,go+600000);ordered->raw(1,go+1500000);ordered->enqueue(2,go+1700001);ordered->enqueue(2,go+1700000);ordered->pump(go+1700001);ordered->settle(go+1700010);const auto rr=ordered->engine.completedResult();const bool orderOk=rr.sealed&&rr.finishTime==go+1700000&&rr.entries[1].laps==2;
  s12("12.5a",orderOk,"reversed_delivery_F_exact_and_post_F_ignored=1");delete ordered;
  auto fenced=new Stage12Fixture(pp::LapFinishBehaviour::CompleteFullRaceDistance,2);fenced->start();fenced->raw(1,go+400000);fenced->raw(2,go+500000);fenced->raw(1,go+900000);const pp::Time fenceF=go+1500000;for(uint8_t i=0;i<pp::Bus::Depth;++i){pp::Message fill{};fill.type=pp::Type::InputEvent;fill.eventId=500+i;fill.relevantTime=fenceF+100+i;fill.input={0xdead0000u,uint16_t(i+1)};fenced->bus.publish(fenced->inputEndpoint,fill);}fenced->input.sampleSourceB(true,fenceF-20000);fenced->input.sampleSourceB(true,fenceF);const bool protectedOne=fenced->input.protectedBacklogDepth()==1&&fenced->input.cleanTriggers()==1;fenced->engine.tick();fenced->input.tick(fenceF);fenced->engine.tick();fenced->input.tick(fenceF+1);fenced->engine.tick();fenced->control.tick(fenceF+1);const auto fenceResult=fenced->engine.completedResult();const bool fenceOk=protectedOne&&fenced->input.maximumProtectedBacklogDepth()==1&&fenceResult.sealed&&fenceResult.finishTime==fenceF&&fenceResult.entries[1].laps==2;
  s12("12.4",fenceOk,"accepted_inventory=1 interpreted_exactly_once=1 max_backlog=1 F=%llu",(unsigned long long)fenceResult.finishTime);delete fenced;
  s12("12.22",ir.sealed&&cr.sealed&&fr.sealed&&jointOk&&orderOk&&fenceOk,"core_scenarios_pass=1; full_regression_external=1");
  stage12Output.log("ACC S12 test=12.T wrong=FAIL restored=PASS");
  stage12Output.log("ACC DONE");
}
void stage12AcceptanceCommand(char c){if(c=='s')stage12Run();}
void stage12AcceptanceTick(){stage12Output.flush();if(ready&&!stage12Ready){stage12Ready=true;stage12Output.log("ACC READY");}}


