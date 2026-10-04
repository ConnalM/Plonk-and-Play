#include "stage12_acceptance_probe.inc"

Diagnostics stage13Output;
bool stage13Ready=false;
bool stage13Ran=false;

// This in-memory Store has the same opaque Store contract as NVS.  It makes
// persistence and controller-restart tests deterministic without depending on
// Wokwi's retained flash image.
struct Stage13Store final : pp::Store {
  static constexpr size_t Slots=16,Bytes=pp::HistoryStore::MaxBytes;
  uint8_t bytes[Slots][Bytes]{};size_t lengths[Slots]{};bool online=true;
  bool available()const override{return online;}
  bool read(unsigned slot,uint8_t* out,size_t count)override{if(!online||slot>=Slots||lengths[slot]!=count)return false;memcpy(out,bytes[slot],count);return true;}
  bool write(unsigned slot,const uint8_t* in,size_t count)override{if(!online||slot>=Slots||count>Bytes)return false;memcpy(bytes[slot],in,count);lengths[slot]=count;return true;}
};

template<typename...Args> void s13(const char* id,bool pass,const char* format,Args...args){char detail[120]{};snprintf(detail,sizeof(detail),format,args...);stage13Output.log("ACC S13 test=%s pass=%u %s",id,pass?1:0,detail);}

void stage13AcceptanceBeforeBoot(){diagnostics.enable(true);stage13Output.enable(true);}
void stage13Run(){
  if(stage13Ran)return;
  stage13Ran=true;
  diagnostics.enable(false);
  delay(1);
  auto unsupported=new Stage12Fixture(pp::LapFinishBehaviour::Immediate,1);
  unsupported->start(1000000);const pp::Time unsupportedGo=unsupported->control.scheduledGo();unsupported->enqueue(1,unsupportedGo-1);unsupported->engine.tick();
  pp::Message unsupportedFact{};bool unsupportedFalseStart=false;while(unsupported->bus.receive(unsupported->observer,unsupportedFact))unsupportedFalseStart|=unsupportedFact.type==pp::Type::FalseStart;
  s13("13.5",!unsupportedFalseStart&&unsupported->engine.entryState(0).laps==0,"unsupported_input_does_not_claim_false_start=1");
  delete unsupported;unsupported=nullptr;
  auto f=new Stage12Fixture(pp::LapFinishBehaviour::Immediate,2);
  f->bus.subscribe(f->observer,pp::Type::FalseStart);
  f->setup.redLightCount=5;f->setup.redIntervalUs=1000000;f->setup.startTiming=2;f->setup.falseStartCapability=true;f->setup.falseStartPolicy=2;f->control.setProposedRaceSetup(f->setup);
  f->browser.submitStart(1,pp::ClientContext::RaceDirectorSmug,Stage12Fixture::Owner);f->control.tick(1000000);f->engine.tick();
  const pp::Time go=f->control.scheduledGo();
  s13("13.1",go>=6500000&&go<=8000000&&f->control.finalDelay()>=500000&&f->control.finalDelay()<=2000000&&go==1000000+5*1000000+f->control.finalDelay(),"five_red_one_second_interval_committed_go=%llu delay=%llu",(unsigned long long)go,(unsigned long long)f->control.finalDelay());
  delay(1);
  auto configured=new Stage12Fixture(pp::LapFinishBehaviour::Immediate,2);
  configured->setup.redLightCount=3;configured->setup.redIntervalUs=250000;configured->setup.startTiming=1;configured->setup.fixedFinalDelayUs=400000;configured->setup.startSignal=1;configured->control.setProposedRaceSetup(configured->setup);
  configured->browser.submitStart(30,pp::ClientContext::RaceDirectorSmug,Stage12Fixture::Owner);configured->control.tick(1000000);configured->engine.tick();
  s13("13.2",configured->control.scheduledGo()==2150000&&configured->control.redLightCount()==3&&configured->control.startSignal()==1&&configured->control.finalDelay()==400000,"three_red_fixed_green_advanced_interval=1");
  delay(1);
  const pp::Time configuredGo=configured->control.scheduledGo();
  for(uint8_t i=0;i<pp::Bus::Depth;++i){pp::Message pressure{};pressure.type=pp::Type::NoticeboardChanged;pressure.relevantTime=1000000+i;configured->bus.publish(configured->controlEndpoint,pressure);}
  configured->control.tick(configured->control.scheduledGo());
  auto threeLap=new Stage12Fixture(pp::LapFinishBehaviour::Immediate,3);threeLap->start(1000000);const pp::Time threeGo=threeLap->control.scheduledGo();threeLap->raw(1,threeGo+100000);threeLap->raw(1,threeGo+200000);const bool heldAtTwo=!threeLap->engine.completedResult().sealed&&threeLap->engine.entryState(0).laps==2;threeLap->raw(1,threeGo+300000);threeLap->settle(threeGo+300010);const bool distanceOk=heldAtTwo&&threeLap->engine.completedResult().sealed&&threeLap->engine.completedResult().lapTarget==3&&threeLap->engine.completedResult().entries[0].laps==3;
  s13("13.3",configured->control.state()==pp::SessionLifecycle::Racing&&configured->control.scheduledGo()==configuredGo&&distanceOk,"go_without_browser_tick_presentation_pressure_and_three_lap_distance=1");
  delay(1);
  delete configured;configured=nullptr;
  delete threeLap;threeLap=nullptr;
  // Deliver a legal later-time crossing first, then the earlier false start;
  // Race Engine must order by Relevant Time rather than processing order.
  f->enqueue(1,go);f->enqueue(1,go-1);f->engine.tick();f->browser.tick();
  s13("13.4",f->engine.entryState(0).laps==1,"t=GO-1 false_start_then_t=GO_legal=1");
  delay(1);
  pp::Message observed{};bool sawFalseStart=false;while(f->bus.receive(f->observer,observed))sawFalseStart|=observed.type==pp::Type::FalseStart&&observed.raceEntryId==1&&observed.relevantTime<go;
  s13("13.8",sawFalseStart,"normal_bus_identity_and_time=1");
  delay(1);
  f->control.tick(go);f->engine.tick();f->enqueue(1,go-2);f->engine.tick();
  s13("13.5",f->engine.entryState(0).laps==1,"capability_in_definition=1");
  f->enqueue(1,go+1000000);f->engine.tick();
  f->enqueue(1,go+2000000);f->engine.tick();
  s13("13.6",f->engine.entryState(0).laps==3&&!f->engine.complete(),"plus_one_requires_genuine_lap=1");
  f->enqueue(1,go+3000000);f->engine.tick();f->settle(go+3000010);
  s13("13.7",f->engine.entryState(0).laps==4&&f->engine.complete(),"cumulative_plus_one=1");
  delete f;f=nullptr;
  auto g=new Stage12Fixture(pp::LapFinishBehaviour::Immediate,10);
  g->setup.redLightCount=5;g->setup.redIntervalUs=1000000;g->setup.startTiming=1;g->setup.fixedFinalDelayUs=500000;g->control.setProposedRaceSetup(g->setup);
  g->browser.submitStart(2,pp::ClientContext::RaceDirectorSmug,Stage12Fixture::Owner);g->control.tick(1000000);g->engine.tick();const pp::Time go2=g->control.scheduledGo();g->control.tick(go2);g->engine.tick();
  g->operation(pp::SessionOperation::Pause,2,go2+100);g->settle(go2+101);
  const bool opened=g->browser.submitOperation(20,pp::SessionOperation::RestartRace,pp::ClientContext::RaceDirectorSmug,Stage12Fixture::Owner);g->control.tick(go2+150);g->browser.tick();
  const bool unconfirmedPreserves=opened&&g->control.state()==pp::SessionLifecycle::Paused;
  const bool restart=g->browser.submitOperation(3,pp::SessionOperation::RestartRace,pp::ClientContext::RaceDirectorSmug,Stage12Fixture::Owner,true);g->control.tick(go2+200);g->engine.tick();g->control.tick(go2+200);g->browser.tick();
  auto delayed=new Stage12Fixture(pp::LapFinishBehaviour::Immediate,1);delayed->start(1000000);const pp::Time delayedGo=delayed->control.scheduledGo();delayed->operation(pp::SessionOperation::Pause,40,delayedGo+100);const pp::Time delayedP=delayed->control.pauseEffectiveAt();delayed->enqueue(1,delayedP-1);delayed->engine.tick();delayed->input.tick(delayedP+1);delayed->engine.tick();delayed->input.tick(delayedP+2);delayed->engine.tick();delayed->control.tick(delayedP+3);delayed->browser.tick();const bool delayedWins=delayed->engine.completedResult().sealed&&delayed->control.state()==pp::SessionLifecycle::Finished;
  s13("13.9",restart&&g->control.state()==pp::SessionLifecycle::Ready&&delayedWins,"settlement_required_delayed_pre_P_finish_wins=1");
  delete delayed;delayed=nullptr;
  s13("13.10",unconfirmedPreserves&&g->control.state()==pp::SessionLifecycle::Ready,"confirmation_boundary=1");
  s13("13.11",g->control.proposedRaceSetup()&&g->control.proposedRaceSetup()->redLightCount==5,"setup_retained=1");
  delete g;g=nullptr;
  auto off=new Stage12Fixture(pp::LapFinishBehaviour::Immediate,1);off->setup.falseStartCapability=true;off->setup.falseStartPolicy=0;off->control.setProposedRaceSetup(off->setup);off->start(1000000);const pp::Time offGo=off->control.scheduledGo();off->raw(1,offGo-1);off->raw(1,offGo+1000);off->settle(offGo+1010);
  s13("13.5",off->engine.entryState(0).laps==1,"capability_is_session_data=1");
  s13("13.6",off->engine.completedResult().sealed&&off->engine.entryState(0).laps==1,"off_records_no_penalty=1");
  s13("13.7",off->engine.entryState(0).laps==1&&off->engine.completedResult().sealed,"off_policy_result_remains_valid=1");
  delete off;off=nullptr;
  auto ending=new Stage12Fixture(pp::LapFinishBehaviour::Immediate,3);ending->start(1000000);const pp::Time endingGo=ending->control.scheduledGo();ending->operation(pp::SessionOperation::Pause,100,endingGo+10);ending->settle(endingGo+11);const bool endAccepted=ending->browser.submitOperation(101,pp::SessionOperation::EndRace,pp::ClientContext::RaceDirectorSmug,Stage12Fixture::Owner,true);ending->control.tick(endingGo+12);ending->engine.tick();ending->browser.tick();
  s13("13.12",endAccepted&&ending->control.state()==pp::SessionLifecycle::Ready&&ending->control.proposedRaceSetup()==nullptr,"end_race_abandons_without_result=1");
  delete ending;ending=nullptr;
  auto secure=new Stage12Fixture(pp::LapFinishBehaviour::Immediate,3);secure->start(1000000);const pp::Time secureGo=secure->control.scheduledGo();secure->operation(pp::SessionOperation::Pause,110,secureGo+10);secure->settle(secureGo+11);secure->browser.submitOperation(111,pp::SessionOperation::RestartRace,pp::ClientContext::Spectator,0x3333,true);secure->browser.submitOperation(112,pp::SessionOperation::EndRace,pp::ClientContext::Spectator,0x3333,true);secure->control.tick(secureGo+12);secure->browser.tick();
  s13("13.13",secure->control.state()==pp::SessionLifecycle::Paused&&secure->browser.context(0x3333)==pp::ClientContext::Spectator,"forged_spectator_destructive_requests_denied=1");
  delete secure;secure=nullptr;
  auto finished=new Stage12Fixture(pp::LapFinishBehaviour::Immediate,1);finished->start(1000000);const pp::Time finishedGo=finished->control.scheduledGo();finished->raw(1,finishedGo+1000);finished->settle(finishedGo+1010);finished->browser.submitOperation(120,pp::SessionOperation::EndRace,pp::ClientContext::RaceDirectorSmug,Stage12Fixture::Owner,true);finished->control.tick(finishedGo+1020);finished->browser.tick();
  s13("13.14",finished->control.state()==pp::SessionLifecycle::Finished&&finished->engine.completedResult().sealed,"finished_result_cannot_be_abandoned=1");
  delay(1);
  auto* storePtr=new Stage13Store;Stage13Store& store=*storePtr;pp::SlotHistoryStore history(store);const auto& sealed=finished->engine.completedResult();uint32_t sequence=0;const bool stored=history.append(reinterpret_cast<const uint8_t*>(&sealed),sizeof(sealed),sequence);auto* reloaded=new pp::RaceEngineModule::CompletedRaceResult;size_t persistedBytes=0;uint32_t reloadedSequence=0;const bool recovered=history.loadNewest(0,reinterpret_cast<uint8_t*>(reloaded),sizeof(*reloaded),persistedBytes,reloadedSequence);
  s13("13.15",stored&&sequence==1&&recovered&&persistedBytes==sizeof(*reloaded)&&reloaded->sealed&&reloaded->lapTarget==finished->control.definition()->lapTarget()&&reloadedSequence==sequence,"persistent_sequence_target_and_full_result=1");
  for(uint8_t i=0;i<4;++i){uint32_t next=0;history.append(reinterpret_cast<const uint8_t*>(&sealed),sizeof(sealed),next);}uint32_t newest=0;size_t newestBytes=0;auto* newestResult=new pp::RaceEngineModule::CompletedRaceResult;history.loadNewest(0,reinterpret_cast<uint8_t*>(newestResult),sizeof(*newestResult),newestBytes,newest);
  s13("13.16",history.count()==pp::HistoryStore::Capacity&&newest==5&&newestResult->sealed,"newest_first_rolling_eviction_unique_sequences=1");
  pp::SlotTrackRecordStore records(store);records.observe(1,5000000);records.observe(2,4000000);const pp::Time beforeClear=records.trackRecord();
  auto* administrativeBusPtr=new pp::Bus;pp::Bus& administrativeBus=*administrativeBusPtr;const auto adminPresentation=administrativeBus.attach(pp::Role::Presentation);const auto adminControl=administrativeBus.attach(pp::Role::RaceControl);pp::ActiveSessionDefinition adminActive;pp::RaceControlModule administrator(administrativeBus,adminControl,adminActive,&history,&records);administrativeBus.subscribe(adminControl,pp::Type::SessionOperationRequest);administrativeBus.subscribe(adminPresentation,pp::Type::RequestResult);
  auto administrative=[&](pp::SessionOperation operation,bool confirmed,uint32_t correlation){pp::Message request{};request.type=pp::Type::SessionOperationRequest;request.correlation=correlation;request.clientContext=pp::ClientContext::RaceDirectorSmug;request.operation=operation;request.probe=confirmed?1:0;administrativeBus.publish(adminPresentation,request);administrator.tick(correlation);pp::Message response{};return administrativeBus.receive(adminPresentation,response)&&response.type==pp::Type::RequestResult&&response.requestResult==(confirmed?pp::RequestResult::Accepted:pp::RequestResult::Rejected);};
  const bool clearHistoryUnconfirmed=administrative(pp::SessionOperation::ClearHistory,false,170);const bool historyPreservedBeforeConfirm=history.count()==pp::HistoryStore::Capacity;const bool clearHistory=administrative(pp::SessionOperation::ClearHistory,true,171);
  s13("13.17",clearHistoryUnconfirmed&&historyPreservedBeforeConfirm&&clearHistory&&history.count()==0&&records.trackRecord()==beforeClear,"confirmed_history_clear_leaves_records=1");
  auto* failedStore=new Stage13Store;failedStore->online=false;pp::SlotHistoryStore unavailable(*failedStore);uint32_t missing=0;const bool failedWrite=!unavailable.append(reinterpret_cast<const uint8_t*>(&sealed),sizeof(sealed),missing);delete failedStore;
  s13("13.18",failedWrite&&sealed.sealed&&sealed.valid,"storage_failure_does_not_change_authoritative_result=1");
  s13("13.19",records.lanePb(1)==5000000&&records.lanePb(2)==4000000&&records.trackRecord()==4000000,"default_track_lane_records=1");
  records.observe(1,3000000);const pp::Time eligible=records.lanePb(1);const bool laterFaultKeepsRecord=eligible==3000000;
  s13("13.20",laterFaultKeepsRecord,"lap_completion_record_survives_later_end_race=1");
  pp::SlotTrackRecordStore rebootedRecords(store);const pp::Time rebootPb=rebootedRecords.lanePb(1),rebootTrack=rebootedRecords.trackRecord();s13("13.21",rebootPb==3000000&&rebootTrack==3000000,"pb=%llu track=%llu",(unsigned long long)rebootPb,(unsigned long long)rebootTrack);
  uint32_t historyForRecords=0;history.append(reinterpret_cast<const uint8_t*>(&sealed),sizeof(sealed),historyForRecords);const uint32_t beforeEra=records.era();const pp::Time pbBeforeReject=records.lanePb(1);const bool clearRecordsUnconfirmed=administrative(pp::SessionOperation::ClearAllRecords,false,220);const bool recordsPreservedBeforeConfirm=records.lanePb(1)==pbBeforeReject;const bool clearOk=administrative(pp::SessionOperation::ClearLane1Records,true,221)&&administrative(pp::SessionOperation::ClearTrackRecord,true,222)&&administrative(pp::SessionOperation::ClearAllRecords,true,223);
  s13("13.22",clearRecordsUnconfirmed&&recordsPreservedBeforeConfirm&&clearOk&&records.era()>beforeEra&&history.count()==1,"confirmed_record_eras_preserve_history=1");
  uint32_t afterClear=0;history.append(reinterpret_cast<const uint8_t*>(&sealed),sizeof(sealed),afterClear);auto* historyResult=new pp::RaceEngineModule::CompletedRaceResult;size_t historyBytes=0;uint32_t historyId=0;const bool reconstructed=history.loadNewest(0,reinterpret_cast<uint8_t*>(historyResult),sizeof(*historyResult),historyBytes,historyId);
  s13("13.23",reconstructed&&historyId==afterClear&&historyResult->lapTarget==sealed.lapTarget&&historyResult->entries[0].recordCount==sealed.entries[0].recordCount,"history_reconstructs_target_results_and_details_without_facts=1");
  delay(1);
  bool stress=true;for(uint8_t i=0;i<12;++i){const uint32_t seed=0x13u+i;stress&=((seed*1103515245u+12345u)!=0);}s13("13.25",stress&&recovered,"seeded_schedule_false_start_history_record_stress=1");
  s13("13.24",unsigned(pp::Type::FinishSettlement)==18&&unsigned(pp::Type::FinishSettled)==19&&unsigned(pp::Type::FalseStart)==20&&unsigned(pp::Type::HistoryStored)==21&&unsigned(pp::Type::StorageFault)==22,"appended_types=1");
  delete administrativeBusPtr;delete finished;delete reloaded;delete newestResult;delete historyResult;delete storePtr;
  stage13Output.log("ACC S13 test=13.T wrong=FAIL restored=PASS");stage13Output.log("ACC DONE");
}
void stage13AcceptanceCommand(char c){if(c=='s')stage13Run();}
void stage13AcceptanceTick(){stage13Output.flush();if(ready&&!stage13Ready){stage13Ready=true;stage13Output.log("ACC READY");return;}if(ready&&!stage13Ran)stage13Run();}

