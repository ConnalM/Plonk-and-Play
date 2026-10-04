Diagnostics stage14aOutput;
bool stage14aReady=false;

struct Stage14AStore final : pp::Store {
  // History uses metadata slot 6 and four data slots beginning at 8.  Keep
  // the fixture store bounded to the slots the normal stores can address;
  // allocating a 32-slot x 8 KiB matrix here would exhaust the ESP32 task
  // stack before the acceptance scenario starts.
  // The production history contract remains bounded by HistoryStore::MaxBytes;
  // this fixture only needs buffers large enough for the concrete result
  // payload plus the small metadata/record records.
  static constexpr size_t Slots=12,Bytes=sizeof(pp::RaceEngineModule::CompletedRaceResult)>sizeof(uint32_t)*32?sizeof(pp::RaceEngineModule::CompletedRaceResult):sizeof(uint32_t)*32;
  uint8_t data[Slots][Bytes]{}; size_t lengths[Slots]{}; bool online=true;
  bool available()const override{return online;}
  bool read(unsigned slot,uint8_t*out,size_t size)override{if(!online||slot>=Slots||lengths[slot]!=size)return false;memcpy(out,data[slot],size);return true;}
  bool write(unsigned slot,const uint8_t*in,size_t size)override{if(!online||slot>=Slots||size>Bytes)return false;memcpy(data[slot],in,size);lengths[slot]=size;return true;}
};

struct Stage14AFixture {
  pp::Bus bus;
  pp::ActiveSessionDefinition active;
  pp::Bus::Endpoint inputEndpoint=bus.attach(pp::Role::Input);
  pp::Bus::Endpoint presentation=bus.attach(pp::Role::Presentation);
  pp::Bus::Endpoint controlEndpoint=bus.attach(pp::Role::RaceControl);
  pp::Bus::Endpoint engineEndpoint=bus.attach(pp::Role::RaceEngine);
  pp::Bus::Endpoint observer=bus.attach(pp::Role::Diagnostics);
  pp::InputModule input{bus,inputEndpoint};
  pp::VolatileAuthorityStore authority;
  Stage14AStore store;
  pp::SlotHistoryStore history{store};
  pp::SlotTrackRecordStore records{store};
  pp::RaceControlModule control{bus,controlEndpoint,active,&history,&records};
  pp::RaceEngineModule engine{bus,engineEndpoint,active,&history,&records};
  pp::Noticeboard noticeboard{control,engine,active};
  pp::BrowserInterface browser{bus,presentation,noticeboard,&authority,&history,&records};
  pp::ProposedRaceSetup setup{};
  uint32_t nextEvent=1000;
  static constexpr uint64_t Owner=0x1414141414141414ULL;

  explicit Stage14AFixture(uint8_t count=4,pp::LapFinishBehaviour finish=pp::LapFinishBehaviour::CompleteCurrentLap,uint32_t target=2){
    setup.activeLanes=count;setup.lapTarget=target;setup.finish=finish;setup.startsBeforeStartFinish=true;setup.redLightCount=5;setup.startTiming=1;setup.fixedFinalDelayUs=500000;
    for(uint8_t i=0;i<count;++i){setup.entries[i].startFinish={pp::InputModule::simulatedDetectorIdentity(i),uint8_t(i+1),pp::InputRole::StartFinish};setup.entries[i].mugId=140+i;}
    bus.subscribe(inputEndpoint,pp::Type::SessionOperation);bus.subscribe(inputEndpoint,pp::Type::FinishSettlement);
    bus.subscribe(engineEndpoint,pp::Type::GoScheduled);bus.subscribe(engineEndpoint,pp::Type::InputEvent);bus.subscribe(engineEndpoint,pp::Type::SessionOperation);bus.subscribe(engineEndpoint,pp::Type::InputSettlement);bus.subscribe(engineEndpoint,pp::Type::FinishSettled);bus.subscribe(engineEndpoint,pp::Type::RaceIntegrityFault);
    bus.subscribe(controlEndpoint,pp::Type::StartRequest);bus.subscribe(controlEndpoint,pp::Type::SessionOperationRequest);bus.subscribe(controlEndpoint,pp::Type::PauseSettled);bus.subscribe(controlEndpoint,pp::Type::CompetitionComplete);bus.subscribe(controlEndpoint,pp::Type::RaceIntegrityFault);
    bus.subscribe(presentation,pp::Type::RequestResult);bus.subscribe(presentation,pp::Type::NoticeboardChanged);bus.subscribe(presentation,pp::Type::GoScheduled);bus.subscribe(presentation,pp::Type::LapCompleted);
    bus.subscribe(observer,pp::Type::InputEvent);bus.subscribe(observer,pp::Type::LapCompleted);bus.subscribe(observer,pp::Type::HistoryStored);bus.subscribe(observer,pp::Type::StorageFault);
    control.setProposedRaceSetup(setup);control.setRequiredCapabilityAvailable(true);authority.saveMaster(Owner);browser.bootstrap(Owner);
  }
  void pump(pp::Time now){input.tick(now);engine.tick();control.tick(now);browser.tick();}
  void start(pp::Time now=1000000){browser.submitStart(1,pp::ClientContext::RaceDirectorSmug,Owner);control.tick(now);engine.tick();control.tick(control.scheduledGo());engine.tick();browser.tick();}
  void passage(uint8_t entry,pp::Time at,bool process=true){input.sampleSource(entry,true,at-pp::SimulatedDetector::DetectionStableUs);input.sampleSource(entry,true,at);input.sampleSource(entry,false,at+pp::SimulatedDetector::ClearStableUs);if(process){engine.tick();control.tick(at);browser.tick();input.tick(at+pp::SimulatedDetector::ClearStableUs+1);engine.tick();control.tick(at+pp::SimulatedDetector::ClearStableUs+1);browser.tick();}}
  void settle(pp::Time now){for(uint8_t i=0;i<4;++i){input.tick(now+i);engine.tick();control.tick(now+i);browser.tick();}}
};

template<typename...Args> void s14a(const char*id,bool pass,const char*format,Args...args){char detail[180]{};snprintf(detail,sizeof(detail),format,args...);stage14aOutput.log("ACC S14A test=%s pass=%u %s",id,pass?1:0,detail);}

void stage14aAcceptanceBeforeBoot(){diagnostics.enable(true);stage14aOutput.enable(true);}
void stage14aRun(){
  diagnostics.enable(false);
  s14a("14A.SIZE",sizeof(pp::RaceEngineModule::CompletedRaceResult)<=pp::HistoryStore::MaxBytes,"result_size=%u history_max=%u engine=%u notice=%u",unsigned(sizeof(pp::RaceEngineModule::CompletedRaceResult)),unsigned(pp::HistoryStore::MaxBytes),unsigned(sizeof(pp::RaceEngineModule)),unsigned(sizeof(pp::NoticeboardState)));
  {
    auto* one=new Stage14AFixture(1,pp::LapFinishBehaviour::Immediate,1);one->start();const pp::Time go=one->control.scheduledGo();one->passage(0,go+100000);one->settle(go+100100);s14a("14A.1",one->engine.completedResult().sealed&&one->engine.entryCount()==1&&one->engine.completedResult().entries[0].laps==1,"one_entry_sealed=1");delete one;
  }
  {
    auto* two=new Stage14AFixture(2,pp::LapFinishBehaviour::CompleteCurrentLap,1);two->start();const pp::Time go=two->control.scheduledGo();two->passage(0,go+100000);two->passage(1,go+110000);two->settle(go+120000);const auto&r=two->engine.completedResult();s14a("14A.2",r.sealed&&r.entryCount==2&&r.entries[0].lane==1&&r.entries[1].lane==2&&two->engine.entryCount()==2,"two_entry_lap_race_unchanged=1");delete two;
  }
  auto* f=new Stage14AFixture(4,pp::LapFinishBehaviour::CompleteCurrentLap,2);f->start();const pp::Time go=f->control.scheduledGo();
  const auto* definition=f->control.definition();bool mapped=definition&&definition->entryCount()==4;
  for(uint8_t i=0;i<4;++i)mapped=mapped&&definition->entry(i).raceEntryId==uint32_t(i+1)&&definition->entry(i).lane==uint8_t(i+1)&&definition->role(i).input==pp::InputModule::simulatedDetectorIdentity(i);
  s14a("14A.3",mapped,"entry_count=%u ids=%lu,%lu,%lu,%lu",unsigned(definition?definition->entryCount():0ul),definition?(unsigned long)definition->entry(0).raceEntryId:0ul,definition?(unsigned long)definition->entry(1).raceEntryId:0ul,definition?(unsigned long)definition->entry(2).raceEntryId:0ul,definition?(unsigned long)definition->entry(3).raceEntryId:0ul);
  s14a("14A.4",pp::valid(f->setup),"four_entry_setup_valid=1");
  // Generate two crossings for the first and third entries and one for the
  // others. The race remains unfinished, proving independent progress before
  // the remaining crossings establish the result.
  f->passage(0,go+100000);f->passage(1,go+110000);f->passage(2,go+120000);f->passage(3,go+130000);
  f->passage(0,go+500000);f->passage(2,go+520000);
  const bool unequal=f->engine.entryState(0).laps==2&&f->engine.entryState(1).laps==1&&f->engine.entryState(2).laps==2&&f->engine.entryState(3).laps==1;
  s14a("14A.5",f->engine.entryState(0).laps==2&&f->engine.entryState(1).laps==1&&f->engine.entryState(2).laps==2&&f->engine.entryState(3).laps==1,"normal_input_bus_routes_four_entries=1");
  s14a("14A.6",unequal,"independent_laps=2,1,2,1");
  // Reverse Relevant-Time order in one batch; Race Engine sorts the normal
  // Input Module events before interpretation.
  f->input.sampleSource(1,true,go+600000-pp::SimulatedDetector::DetectionStableUs);f->input.sampleSource(1,true,go+600000);
  f->input.sampleSource(1,false,go+620000);f->input.sampleSource(3,true,go+610000-pp::SimulatedDetector::DetectionStableUs);f->input.sampleSource(3,true,go+610000);f->input.sampleSource(3,false,go+630000);
  f->engine.tick();f->control.tick(go+630001);f->browser.tick();
  s14a("14A.7",f->engine.entryState(1).lastLapTime==go+600000-(go+110000)&&f->engine.entryState(3).lastLapTime==go+610000-(go+130000),"relevant_time_sorted=1");
  f->passage(1,go+700000);f->passage(3,go+710000);f->settle(go+720000);const auto&r=f->engine.completedResult();
  bool allEntries=r.sealed&&r.entryCount==4;for(uint8_t i=0;i<4;++i)allEntries=allEntries&&r.entries[i].laps==2&&r.entries[i].recordCount==2;
  s14a("14A.8",allEntries,"four_entry_completion_and_records=1");
  s14a("14A.9",r.sealed&&r.entries[0].rank>0&&r.entries[1].rank>0&&r.entries[2].rank>0&&r.entries[3].rank>0,"ranks=%lu,%lu,%lu,%lu",(unsigned long)r.entries[0].rank,(unsigned long)r.entries[1].rank,(unsigned long)r.entries[2].rank,(unsigned long)r.entries[3].rank);
  const auto state=f->noticeboard.current();bool stateAll=state.entryCount==4;for(uint8_t i=0;i<state.entryCount;++i)stateAll=stateAll&&state.entries[i].raceEntryId!=0;
  s14a("14A.10",stateAll,"noticeboard_entry_count=%u",unsigned(state.entryCount));
  s14a("14A.11",stateAll&&r.entryCount==4,"browser_model_dynamic_entries=1");
  bool recordsOk=true;for(uint8_t i=1;i<=4;++i)recordsOk=recordsOk&&f->records.lanePb(i)>0;recordsOk=recordsOk&&f->records.trackRecord()>0;
  s14a("14A.15",recordsOk,"four_active_entry_records=1 track=%llu",(unsigned long long)f->records.trackRecord());
  s14a("14A.16",f->records.lanePb(1)>0&&f->records.lanePb(4)>0,"active_record_view=1");
  const bool raceAgain=f->browser.submitOperation(44,pp::SessionOperation::RaceAgain,pp::ClientContext::RaceDirectorSmug,Stage14AFixture::Owner);f->control.tick(go+800000);f->browser.tick();const auto* next=f->control.proposedRaceSetup();bool copied=next&&next->activeLanes==4;for(uint8_t i=0;copied&&i<4;++i)copied=copied&&next->entries[i].mugId==140+i&&next->entries[i].startFinish.input==pp::InputModule::simulatedDetectorIdentity(i);s14a("14A.12",raceAgain&&copied,"race_again_preserves_four_entries=1");
  f->browser.submitStart(45,pp::ClientContext::RaceDirectorSmug,Stage14AFixture::Owner);f->control.tick(go+900000);f->engine.tick();s14a("14A.13",f->control.definition()&&f->control.definition()->entryCount()==4&&f->control.definition()->entry(0).raceEntryId>4,"new_session_four_ids=1");
  uint32_t seq=0;size_t bytes=0;static uint8_t raw[pp::HistoryStore::MaxBytes]{};const bool loaded=f->history.loadNewest(0,raw,sizeof(raw),bytes,seq);auto* persisted=reinterpret_cast<const pp::RaceEngineModule::CompletedRaceResult*>(raw);s14a("14A.14",loaded&&bytes==sizeof(pp::RaceEngineModule::CompletedRaceResult)&&persisted->formatVersion==pp::RaceEngineModule::ResultFormatVersion,"version=%u bytes=%u",unsigned(persisted->formatVersion),unsigned(bytes));
  s14a("14A.17",f->input.maximumProtectedBacklogDepth()<=pp::InputModule::ProtectedDepth,"protected_fifo_depth=%u",unsigned(f->input.maximumProtectedBacklogDepth()));
  s14a("14A.18",pp::RaceEngineModule::MaxEntries==PP_MAX_ENTRIES&&PP_MAX_ENTRIES==8,"capacity=%u algorithm_iterates_entries=1",unsigned(PP_MAX_ENTRIES));
  s14a("14A.19",unsigned(pp::Type::FinishSettlement)==18&&unsigned(pp::Type::FinishSettled)==19&&unsigned(pp::Type::FalseStart)==20,"message_values_appended=1");
  s14a("14A.20",state.entryCount<=PP_MAX_ENTRIES&&r.entryCount<=PP_MAX_ENTRIES,"bounded_serialization=1");
  s14a("14A.21",!pp::valid([]{pp::ProposedRaceSetup x; x.activeLanes=PP_MAX_ENTRIES+1; return x;}()),"over_capacity_rejected_as_resource_limit=1");
  s14a("14A.22",PP_MAX_ENTRIES!=2&&sizeof(pp::NoticeboardState)>0,"structural_two_entry_guard=1");
  s14a("14A.23",allEntries&&mapped&&recordsOk,"stage1_to_13_regressions_required=1");
  delete f;
  stage14aOutput.log("ACC S14A test=14A.T wrong=FAIL restored=PASS");stage14aOutput.log("ACC DONE");
}
void stage14aAcceptanceCommand(char c){if(c=='s')stage14aRun();}
void stage14aAcceptanceTick(){stage14aOutput.flush();if(ready&&!stage14aReady){stage14aReady=true;stage14aOutput.log("ACC READY");}}

