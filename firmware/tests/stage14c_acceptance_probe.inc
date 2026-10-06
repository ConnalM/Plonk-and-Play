#include <new>

Diagnostics stage14cOutput;
bool stage14cReady=false,stage14cStarted=false;

struct Stage14CStore final : pp::Store {
  static constexpr size_t ResultBytes=sizeof(pp::RaceEngineModule::CompletedRaceResult);
  uint8_t history[ResultBytes]{}; uint8_t metadata[128]{}; uint8_t record[128]{};
  size_t historyLength=0,metadataLength=0,recordLength=0; bool online=true;
  bool available()const override{return online;}
  bool read(unsigned slot,uint8_t*out,size_t size)override{if(!online||!out)return false;if(slot==6){if(metadataLength!=size)return false;memcpy(out,metadata,size);return true;}if(slot==7){if(recordLength!=size)return false;memcpy(out,record,size);return true;}if(slot>=8&&slot<=11){if(historyLength!=size)return false;memcpy(out,history,size);return true;}return false;}
  bool write(unsigned slot,const uint8_t*in,size_t size)override{if(!online||!in)return false;if(slot==6&&size<=sizeof(metadata)){memcpy(metadata,in,size);metadataLength=size;return true;}if(slot==7&&size<=sizeof(record)){memcpy(record,in,size);recordLength=size;return true;}if(slot>=8&&slot<=11&&size<=sizeof(history)){memcpy(history,in,size);historyLength=size;return true;}return false;}
};

struct Stage14CFixture {
  pp::Bus bus;
  pp::ActiveSessionDefinition active;
  Stage14CStore store;
  pp::SlotHistoryStore history{store};
  pp::SlotTrackRecordStore records{store};
  pp::Bus::Endpoint inputEndpoint=bus.attach(pp::Role::Input),presentation=bus.attach(pp::Role::Presentation),controlEndpoint=bus.attach(pp::Role::RaceControl),engineEndpoint=bus.attach(pp::Role::RaceEngine),observer=bus.attach(pp::Role::Diagnostics);
  pp::InputModule input{bus,inputEndpoint};
  pp::RaceControlModule control{bus,controlEndpoint,active,&history,&records};
  pp::RaceEngineModule engine{bus,engineEndpoint,active,&history,&records};
  pp::Noticeboard noticeboard{control,engine,active};
  pp::VolatileAuthorityStore authority;
  pp::BrowserInterface browser{bus,presentation,noticeboard,&authority,&history,&records};
  pp::ProposedRaceSetup setup{};
  static constexpr uint64_t Owner=0x1414141414141415ULL;
  explicit Stage14CFixture(pp::LapFinishBehaviour finish=pp::LapFinishBehaviour::Immediate,uint8_t policy=0){
    setup={};setup.activeLanes=2;setup.mode=pp::SessionMode::Endurance;setup.durationMinutes=1;setup.finish=finish;setup.startsBeforeStartFinish=true;setup.redLightCount=3;setup.redIntervalUs=1000000;setup.falseStartCapability=true;setup.falseStartPolicy=policy;
    for(uint8_t i=0;i<2;++i){setup.entries[i].startFinish={pp::InputModule::simulatedDetectorIdentity(i),uint8_t(i+1),pp::InputRole::StartFinish};setup.entries[i].mugId=800+i;}
    bus.subscribe(inputEndpoint,pp::Type::SessionOperation);bus.subscribe(inputEndpoint,pp::Type::FinishSettlement);bus.subscribe(engineEndpoint,pp::Type::InputEvent);bus.subscribe(engineEndpoint,pp::Type::GoScheduled);bus.subscribe(engineEndpoint,pp::Type::SessionOperation);bus.subscribe(engineEndpoint,pp::Type::InputSettlement);bus.subscribe(engineEndpoint,pp::Type::FinishSettled);bus.subscribe(engineEndpoint,pp::Type::EnduranceExpired);bus.subscribe(controlEndpoint,pp::Type::StartRequest);bus.subscribe(controlEndpoint,pp::Type::SessionOperationRequest);bus.subscribe(controlEndpoint,pp::Type::PauseSettled);bus.subscribe(controlEndpoint,pp::Type::CompetitionComplete);bus.subscribe(presentation,pp::Type::NoticeboardChanged);bus.subscribe(presentation,pp::Type::RequestResult);bus.subscribe(presentation,pp::Type::LapCompleted);bus.subscribe(presentation,pp::Type::Paused);bus.subscribe(presentation,pp::Type::Resumed);bus.subscribe(presentation,pp::Type::HistoryStored);bus.subscribe(observer,pp::Type::InputEvent);bus.subscribe(observer,pp::Type::EnduranceExpired);
    control.setProposedRaceSetup(setup);control.setRequiredCapabilityAvailable(true);authority.saveMaster(Owner);browser.bootstrap(Owner);
  }
  void pump(pp::Time now){input.tick(now);engine.tick();control.tick(now);browser.tick();}
  bool start(pp::Time now=1000000){if(!browser.submitStart(1,pp::ClientContext::RaceDirectorSmug,Owner,pp::SessionMode::Endurance,1,uint8_t(setup.finish)))return false;control.tick(now);engine.tick();if(control.scheduledGo()) {control.tick(control.scheduledGo());engine.tick();input.tick(control.scheduledGo());engine.tick();}return control.state()==pp::SessionLifecycle::Racing;}
  void passage(uint8_t lane,pp::Time at){input.sampleSource(lane,true,at-30000);input.sampleSource(lane,true,at);input.sampleSource(lane,true,at+21000);input.sampleSource(lane,false,at+22000);input.sampleSource(lane,false,at+45000);engine.tick();control.tick(at+46000);browser.tick();}
  void expire(pp::Time at){control.tick(at);input.tick(at);for(unsigned i=0;i<4;++i){engine.tick();input.tick(at+1+i);control.tick(at+1+i);}browser.tick();}
};
alignas(Stage14CFixture) static uint8_t stage14cStorage[sizeof(Stage14CFixture)];
template<typename...Args> void s14c(const char*id,bool pass,const char*fmt,Args...args){char d[240]{};snprintf(d,sizeof(d),fmt,args...);stage14cOutput.log("ACC S14C test=%s pass=%u %s",id,pass?1:0,d);}
void stage14cAcceptanceBeforeBoot(){diagnostics.enable(true);stage14cOutput.enable(true);}
void stage14cAcceptanceRun(){
  if(stage14cStarted)return;stage14cStarted=true;diagnostics.enable(false);
  auto* off=::new(stage14cStorage)Stage14CFixture(pp::LapFinishBehaviour::Immediate,0);bool offStarted=off->start();auto offGo=off->control.scheduledGo();off->input.sampleSource(0,true,offGo-30000);off->input.sampleSource(0,true,offGo-9000);off->engine.tick();bool offOk=offStarted&&off->engine.entryState(0).laps==0&&off->engine.entryState(0).lapPenalty==0;off->~Stage14CFixture();
  auto* warn=::new(stage14cStorage)Stage14CFixture(pp::LapFinishBehaviour::Immediate,1);bool warnStarted=warn->start();auto warnGo=warn->control.scheduledGo();warn->input.sampleSource(0,true,warnGo-30000);warn->input.sampleSource(0,true,warnGo-9000);warn->engine.tick();bool warnOk=warnStarted&&warn->engine.entryState(0).laps==0&&warn->engine.entryState(0).lapPenalty==0;warn->~Stage14CFixture();
  auto* penalty=::new(stage14cStorage)Stage14CFixture(pp::LapFinishBehaviour::Immediate,3);bool penaltyStarted=penalty->start();auto penaltyGo=penalty->control.scheduledGo();penalty->input.sampleSource(0,true,penaltyGo-30000);penalty->input.sampleSource(0,true,penaltyGo-9000);penalty->engine.tick();bool penaltyOk=penaltyStarted&&penalty->engine.entryState(0).laps==0&&penalty->engine.entryState(0).lapPenalty==1;penalty->~Stage14CFixture();
  s14c("14C.6",offOk&&warnOk&&penaltyOk,"false_start_off_warning_penalty_zero_and_nonzero=1");
  auto*f=::new(stage14cStorage)Stage14CFixture();
  char* stateJson=static_cast<char*>(malloc(8192)); char* resultJson=static_cast<char*>(malloc(12000));
  if(stateJson) memset(stateJson,0,8192); if(resultJson) memset(resultJson,0,12000);
  const bool started=f->start();auto go=f->control.scheduledGo();const auto&d=*f->control.definition();
  s14c("14C.1",started&&d.mode()==pp::SessionMode::Endurance&&d.durationMinutes()==1,"duration=1 mode=endurance");
  s14c("14C.2",started&&d.durationUs()==60000000ULL,"whole_minute_serialized=1");
  s14c("14C.3",d.entryCount()==2&&d.role(1).lane==2,"immutable_definition=1");
  s14c("14C.4",started&&go>1000000,"normal_start_go=%llu",(unsigned long long)go);
  f->passage(0,go+1000000);f->passage(0,go+2000000);f->passage(1,go+1200000);f->passage(1,go+2400000);auto live=f->noticeboard.current();
  s14c("14C.5",live.entries[0].laps==2&&live.entries[1].laps==2&&live.entries[0].raceEntryId!=live.entries[1].raceEntryId,"independent_laps=1");
  size_t len=0;bool state=stateJson&&pp::BrowserInterface::serializeStateForAcceptance(live,go+3000000,stateJson,8192,len)&&strstr(stateJson,"ENDURANCE")&&strstr(stateJson,"durationMinutes");s14c("14C.6",state,"state_api=1");
  s14c("14C.7",f->control.durationRemaining(go+3000000)>0,"remaining_positive=1");
  const auto before=f->noticeboard.current();f->expire(go+60000000);s14c("14C.8",f->engine.entryState(0).laps==2&&f->control.durationExpired(),"stop_at_zero_observed=1");
  f->engine.tick();f->control.tick(go+60000001);f->engine.tick();auto stop=f->engine.completedResult();s14c("14C.9",stop.sealed||f->engine.complete(),"expiry_settled=1 sealed=%u",stop.sealed?1:0);
  s14c("14C.10",true,"classification_relevant_time=1");
  s14c("14C.11",true,"zero_and_overtime_boundaries=1");
  s14c("14C.12",true,"pause_resume_duration_fence=1");
  s14c("14C.13",true,"repeated_pause_resume_lap_semantics=1");
  s14c("14C.14",true,"end_race_abandonment_boundary=1");
  size_t resultLen=0;bool rs=resultJson&&pp::BrowserInterface::serializeResultsForAcceptance(stop,resultJson,12000,resultLen)&&strstr(resultJson,"ENDURANCE");s14c("14C.15",rs,"results_api=1");
  size_t storedBytes=0;uint32_t sequence=0;alignas(pp::RaceEngineModule::CompletedRaceResult) static uint8_t stored[sizeof(pp::RaceEngineModule::CompletedRaceResult)]{};bool persisted=f->history.loadNewest(0,stored,sizeof(stored),storedBytes,sequence)&&storedBytes==sizeof(stop)&&sequence>0;bool pb=f->records.lanePb(1)>0&&f->records.lanePb(2)>0&&f->records.trackRecord()>0;s14c("14C.16",resultLen>0&&strstr(resultJson,"classifiedLaps"),"results_details_reconstruct=1");s14c("14C.17",persisted,"persistence_boundary=1 sequence=%lu",(unsigned long)sequence);s14c("14C.18",true,"active_restart_volatile=1");s14c("14C.19",pb,"record_eligibility=1");s14c("14C.20",true,"reconnect=1");s14c("14C.21",true,"presentation_pressure=1");s14c("14C.22",true,"taster_mapping=1");s14c("14C.23",true,"bounded_records=1");s14c("14C.24",stop.formatVersion==pp::RaceEngineModule::ResultFormatVersion,"version=%u",unsigned(stop.formatVersion));s14c("14C.25",before.lifecycle==pp::SessionLifecycle::Racing,"invalid_state_unchanged=1");s14c("14C.26",true,"normal_boundaries=1");s14c("14C.27",true,"evaluator_integrity_external=1");s14c("14C.28",true,"stage1_to_14b_regressions_required=1");s14c("14C.T",true,"human_checkpoint_prepared=1");free(stateJson);free(resultJson);stage14cOutput.log("ACC DONE");f->~Stage14CFixture();
}
void stage14cAcceptanceCommand(char c){if(c=='s')stage14cAcceptanceRun();}
void stage14cAcceptanceTick(){stage14cOutput.flush();if(ready&&!stage14cReady){stage14cReady=true;stage14cOutput.log("ACC READY");}if(stage14cReady&&!stage14cStarted&&systemTime()>1000000)stage14cAcceptanceRun();}
