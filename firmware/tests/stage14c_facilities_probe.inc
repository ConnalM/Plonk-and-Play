// Focused acceptance for the P&P-owned presentation facilities.  These tests
// use the same Race Control, Race Engine, Noticeboard and Browser serializers
// as the production routes; no Browser-side state is substituted.
static void stage14cFacilitiesAcceptanceRun(){
  auto* readiness=::new(stage14cStorage)Stage14CFixture();
  char* proposalJson=static_cast<char*>(malloc(4096)); size_t proposalLength=0; if(proposalJson)memset(proposalJson,0,4096);
  const auto readyState=readiness->noticeboard.current();
  const bool readyNone=readyState.lifecycle==pp::SessionLifecycle::Ready&&readyState.sessionMode==pp::SessionMode::None;
  const bool proposal=proposalJson&&readiness->browser.serializeProposalForAcceptance(proposalJson,4096,proposalLength)&&proposalLength>0&&strstr(proposalJson,"\"formatVersion\":1")&&strstr(proposalJson,"\"readinessContext\"")&&strstr(proposalJson,"\"severity\":\"NONE\"")&&strstr(proposalJson,"\"mode\":\"ENDURANCE\"")&&strstr(proposalJson,"\"mode\":\"OPEN_PRACTICE\",\"available\":true")&&strstr(proposalJson,"\"mode\":\"TIMED_STAGE\",\"available\":false");
  s14c("14C.P1",readyNone&&proposal,"readiness_and_capabilities=%u proposal_bytes=%lu",proposal?1:0,(unsigned long)proposalLength);
  free(proposalJson); readiness->~Stage14CFixture();

  auto* start=::new(stage14cStorage)Stage14CFixture();
  const bool startSubmitted=start->browser.submitStart(2101,pp::ClientContext::RaceDirectorSmug,Stage14CFixture::Owner);
  start->control.tick(1000000); start->engine.tick();
  const pp::Time startGo=start->control.scheduledGo();
  const auto countdown=start->noticeboard.current();
  start->control.tick(startGo); start->engine.tick();
  const auto go=start->noticeboard.current();
  char* startJson=static_cast<char*>(malloc(8192)); size_t startLength=0; if(startJson)memset(startJson,0,8192);
  const bool startReload=startJson&&start->browser.serializeStateForAcceptance(countdown,1000000,startJson,8192,startLength)&&strstr(startJson,"\"startPresentation\":{\"phase\":\"COUNTDOWN\"")&&strstr(startJson,"\"active\":true");
  const bool startPresentation=startSubmitted&&countdown.lifecycle==pp::SessionLifecycle::Starting&&countdown.startPhase==pp::StartPresentationPhase::Countdown&&countdown.startPhaseUntil==startGo&&go.startPhase==pp::StartPresentationPhase::Go&&go.startPhaseUntil==startGo&&startReload;
  s14c("14C.P2",startPresentation,"start_presentation_countdown_go=%u",startPresentation?1:0);
  free(startJson);
  start->~Stage14CFixture();

  auto* resume=::new(stage14cStorage)Stage14CFixture();
  const bool resumeStarted=resume->start(); const pp::Time resumeBase=resume->control.scheduledGo();
  resume->passage(0,resumeBase+1000000);
  resume->browser.submitOperation(2102,pp::SessionOperation::Pause,pp::ClientContext::RaceDirectorSmug,Stage14CFixture::Owner);
  resume->control.tick(resumeBase+3000000); resume->engine.tick(); resume->settle(resumeBase+3010000);
  resume->browser.submitOperation(2103,pp::SessionOperation::Resume,pp::ClientContext::RaceDirectorSmug,Stage14CFixture::Owner);
  const pp::Time resumeRequest=resumeBase+5000000; resume->control.tick(resumeRequest); resume->engine.tick();
  const auto resumeState=resume->noticeboard.current();
  char* resumeJson=static_cast<char*>(malloc(8192)); size_t resumeLength=0; if(resumeJson)memset(resumeJson,0,8192);
  const bool resumeReload=resumeJson&&resume->browser.serializeStateForAcceptance(resumeState,resumeRequest,resumeJson,8192,resumeLength)&&strstr(resumeJson,"\"startPresentation\":{\"phase\":\"RESUME_COUNTDOWN\"")&&strstr(resumeJson,"\"active\":true");
  const bool resumePresentation=resumeStarted&&resumeState.lifecycle==pp::SessionLifecycle::Starting&&resumeState.startPhase==pp::StartPresentationPhase::ResumeCountdown&&resumeState.startPhaseUntil>resumeRequest&&resumeReload;
  s14c("14C.P3",resumePresentation,"resume_presentation_reconstructable=%u phase=%u",resumePresentation?1:0,unsigned(resumeState.startPhase));
  free(resumeJson);
  resume->~Stage14CFixture();

  auto* practice=::new(stage14cStorage)Stage14CFixture();
  practice->setup.mode=pp::SessionMode::OpenPractice; practice->setup.lapTarget=0; practice->setup.durationMinutes=0;
  practice->control.setProposedRaceSetup(practice->setup);
  const bool practiceStarted=practice->browser.submitStart(2104,pp::ClientContext::RaceDirectorSmug,Stage14CFixture::Owner);
  practice->control.tick(2000000); practice->engine.tick();
  const auto practiceRunning=practice->noticeboard.current();
  practice->passage(0,3000000); practice->passage(0,5000000); practice->passage(1,3200000); practice->passage(1,5200000);
  practice->browser.submitOperation(2105,pp::SessionOperation::EndSession,pp::ClientContext::RaceDirectorSmug,Stage14CFixture::Owner);
  practice->control.tick(6000000); practice->engine.tick(); practice->browser.tick();
  const auto& summary=practice->noticeboard.practiceSummary();
  char* summaryJson=static_cast<char*>(malloc(2048)); size_t summaryLength=0; if(summaryJson)memset(summaryJson,0,2048);
  const bool summaryApi=summaryJson&&practice->browser.serializePracticeSummaryForAcceptance(summaryJson,2048,summaryLength)&&summaryLength>0&&strstr(summaryJson,"\"available\":true")&&strstr(summaryJson,"\"lastLap\":2000000")&&strstr(summaryJson,"\"bestLap\":2000000")&&strstr(summaryJson,"\"sessionFastestEntryId\"");
  const bool summaryReload=summaryApi&&practice->browser.serializePracticeSummaryForAcceptance(summaryJson,2048,summaryLength)&&strstr(summaryJson,"\"sessionId\":")&&strstr(summaryJson,"\"entries\":[");
  const bool practiceSummary=practiceStarted&&practiceRunning.lifecycle==pp::SessionLifecycle::Racing&&practiceRunning.startPhase==pp::StartPresentationPhase::None&&summary.available&&summary.sessionId!=0&&summary.entryCount==2&&summary.entries[0].laps==1&&summary.entries[1].laps==1&&summary.entries[0].lastLap==2000000&&summary.entries[0].bestLap==2000000&&summaryApi&&summaryReload&&practice->control.state()==pp::SessionLifecycle::Ready&&practice->engine.completedResult().sealed==false&&practice->engine.historySequence()==0;
  s14c("14C.P4",practiceSummary,"practice_summary_nonofficial=%u json_bytes=%lu",practiceSummary?1:0,(unsigned long)summaryLength);
  free(summaryJson); practice->~Stage14CFixture();

  auto* finish=::new(stage14cStorage)Stage14CFixture();
  finish->setup.mode=pp::SessionMode::LapRace; finish->setup.lapTarget=1; finish->setup.durationMinutes=0; finish->setup.finish=pp::LapFinishBehaviour::Immediate; finish->setup.redLightCount=3;
  finish->control.setProposedRaceSetup(finish->setup);
  const bool finishStarted=finish->browser.submitStart(2106,pp::ClientContext::RaceDirectorSmug,Stage14CFixture::Owner);
  finish->control.tick(7000000); finish->engine.tick(); const pp::Time finishGo=finish->control.scheduledGo();
  finish->control.tick(finishGo); finish->engine.tick(); finish->passage(0,finishGo+1000000);
  finish->input.tick(finishGo+1100000); finish->engine.tick(); finish->control.tick(finishGo+1200000); finish->engine.tick(); finish->control.tick(finishGo+1300000); finish->browser.tick();
  const auto finished=finish->noticeboard.current();
  char* stateJson=static_cast<char*>(malloc(8192)); size_t stateLength=0; if(stateJson)memset(stateJson,0,8192); const bool stateApi=stateJson&&finish->browser.serializeStateForAcceptance(finished,finishGo+1300000,stateJson,8192,stateLength)&&strstr(stateJson,"\"finishDisplay\":{\"active\":true")&&strstr(stateJson,"\"durationSeconds\":5")&&strstr(stateJson,"\"resultSealed\":true");
  const bool finishReload=stateApi&&finish->browser.serializeStateForAcceptance(finished,finishGo+1200000,stateJson,8192,stateLength)&&strstr(stateJson,"\"finishDisplay\":{\"active\":true")&&strstr(stateJson,"\"resultSealed\":true");
  const bool finishDefault=finishStarted&&finished.lifecycle==pp::SessionLifecycle::Finished&&finished.resultSealed&&finished.finishDisplayActive&&finished.finishDisplayDurationSeconds==5&&finished.finishDisplayRemaining>0&&stateApi&&finishReload;
  finish->browser.submitOperation(2107,pp::SessionOperation::SkipFinishDisplay,pp::ClientContext::RaceDirectorSmug,Stage14CFixture::Owner); finish->control.tick(finishGo+1400000); const auto skipped=finish->noticeboard.current();
  const bool skippedOk=!skipped.finishDisplayActive&&skipped.resultSealed&&finish->engine.historySequence()==1;
  s14c("14C.P5",finishDefault&&skippedOk,"finish_display_default_skip=%u remaining_us=%llu",finishDefault&&skippedOk?1:0,(unsigned long long)finished.finishDisplayRemaining);
  free(stateJson); finish->~Stage14CFixture();

  auto* zero=::new(stage14cStorage)Stage14CFixture(); zero->setup.mode=pp::SessionMode::LapRace; zero->setup.lapTarget=1; zero->setup.durationMinutes=0; zero->control.setProposedRaceSetup(zero->setup); zero->control.setFinishDisplayDurationSeconds(0);
  const bool zeroStarted=zero->browser.submitStart(2108,pp::ClientContext::RaceDirectorSmug,Stage14CFixture::Owner); zero->control.tick(8000000); zero->engine.tick(); const pp::Time zeroGo=zero->control.scheduledGo(); zero->control.tick(zeroGo); zero->engine.tick(); zero->passage(0,zeroGo+1000000); zero->input.tick(zeroGo+1100000); zero->engine.tick(); zero->control.tick(zeroGo+1200000); zero->engine.tick(); zero->control.tick(zeroGo+1300000); const auto zeroState=zero->noticeboard.current(); const bool zeroOk=zeroStarted&&zeroState.lifecycle==pp::SessionLifecycle::Finished&&zeroState.resultSealed&&!zeroState.finishDisplayActive&&zeroState.finishDisplayDurationSeconds==0; s14c("14C.P6",zeroOk,"finish_display_zero=%u result_unchanged=%u",zeroOk?1:0,zeroState.resultSealed?1:0); zero->~Stage14CFixture();

  // Live position and gap are copied from the Race Engine into the
  // lane-ordered Noticeboard/API view.  The crossing order changes sporting
  // position while the physical lane identities remain fixed.
  auto* live=::new(stage14cStorage)Stage14CFixture(); live->setup.mode=pp::SessionMode::LapRace; live->setup.lapTarget=3; live->setup.durationMinutes=0; live->setup.finish=pp::LapFinishBehaviour::Immediate; live->control.setProposedRaceSetup(live->setup);
  const bool liveSubmitted=live->browser.submitStart(2109,pp::ClientContext::RaceDirectorSmug,Stage14CFixture::Owner,pp::SessionMode::LapRace,3,0); live->control.tick(20000000); live->engine.tick(); const pp::Time liveGo=live->control.scheduledGo(); live->control.tick(liveGo); live->engine.tick();
  live->passage(1,liveGo+1000000); const auto afterLane2=live->noticeboard.current(); live->passage(0,liveGo+2000000); live->passage(0,liveGo+3000000); const auto afterLane1=live->noticeboard.current();
  char* liveJson=static_cast<char*>(malloc(8192)); size_t liveLength=0; if(liveJson)memset(liveJson,0,8192); const bool liveApi=liveJson&&live->browser.serializeStateForAcceptance(afterLane1,liveGo+3000000,liveJson,8192,liveLength)&&strstr(liveJson,"\"lane\":1,\"position\":1")&&strstr(liveJson,"\"lane\":2,\"position\":2")&&strstr(liveJson,"\"lane\":2,\"position\":2,\"lapsBehind\":1");
  const bool liveOrder=liveSubmitted&&afterLane2.entries[0].lane==1&&afterLane2.entries[1].lane==2&&afterLane2.entries[0].position==2&&afterLane2.entries[1].position==1&&afterLane1.entries[0].position==1&&afterLane1.entries[1].position==2&&afterLane1.entries[1].lapsBehind==1&&liveApi; s14c("14C.P7",liveOrder,"live_position_lane_order=%u api=%u",liveOrder?1:0,liveApi?1:0); free(liveJson); live->~Stage14CFixture();

  auto* practicePosition=::new(stage14cStorage)Stage14CFixture(); practicePosition->setup.mode=pp::SessionMode::OpenPractice; practicePosition->setup.lapTarget=0; practicePosition->setup.durationMinutes=0; practicePosition->control.setProposedRaceSetup(practicePosition->setup); const bool practicePositionStarted=practicePosition->browser.submitStart(2110,pp::ClientContext::RaceDirectorSmug,Stage14CFixture::Owner,pp::SessionMode::OpenPractice,0,0); practicePosition->control.tick(30000000); practicePosition->engine.tick(); const auto practiceState=practicePosition->noticeboard.current(); const bool practiceNoCompetition=practicePositionStarted&&practiceState.sessionMode==pp::SessionMode::OpenPractice&&practiceState.startPhase==pp::StartPresentationPhase::None&&practiceState.entries[0].position==0&&practiceState.entries[0].lapsBehind==0&&practiceState.entries[1].position==0&&practiceState.entries[1].lapsBehind==0; s14c("14C.P8",practiceNoCompetition,"practice_no_position_gap=%u",practiceNoCompetition?1:0); practicePosition->~Stage14CFixture();

  auto* lights=::new(stage14cStorage)Stage14CFixture(); const bool lightsSubmitted=lights->browser.submitStart(2111,pp::ClientContext::RaceDirectorSmug,Stage14CFixture::Owner); lights->control.tick(40000000); lights->engine.tick(); const pp::Time lightsGo=lights->control.scheduledGo(); const pp::Time lightsFirst=lightsGo-pp::Time(lights->control.redLightCount())*lights->control.definition()->redIntervalUs()-lights->control.finalDelay(); const auto lightsBefore=([&](){lights->control.tick(lightsFirst?lightsFirst-1:0); return lights->noticeboard.currentAt(lightsFirst?lightsFirst-1:0);})(); lights->control.tick(lightsFirst); const auto lightsOne=lights->noticeboard.currentAt(lightsFirst); lights->control.tick(lightsFirst+lights->control.definition()->redIntervalUs()); const auto lightsTwo=lights->noticeboard.currentAt(lightsFirst+lights->control.definition()->redIntervalUs()); lights->control.tick(lightsGo); const auto lightsGoState=lights->noticeboard.currentAt(lightsGo); const bool lightsSequence=lightsSubmitted&&lightsBefore.startPhase==pp::StartPresentationPhase::Countdown&&lightsBefore.redLightsLit==0&&lightsOne.redLightsLit==1&&lightsTwo.redLightsLit==2&&lightsGoState.startPhase==pp::StartPresentationPhase::Go&&lightsGoState.redLightsLit==0; s14c("14C.P9",lightsSequence,"authoritative_start_steps=%u values=%u/%u/%u/%u",lightsSequence?1:0,unsigned(lightsBefore.redLightsLit),unsigned(lightsOne.redLightsLit),unsigned(lightsTwo.redLightsLit),unsigned(lightsGoState.redLightsLit)); lights->~Stage14CFixture();
  auto* resumeLights=::new(stage14cStorage)Stage14CFixture(); const bool resumeSubmitted=resumeLights->browser.submitStart(2112,pp::ClientContext::RaceDirectorSmug,Stage14CFixture::Owner); resumeLights->control.tick(50000000); resumeLights->engine.tick(); const pp::Time resumeInitialGo=resumeLights->control.scheduledGo(); resumeLights->control.tick(resumeInitialGo); resumeLights->engine.tick(); resumeLights->browser.submitOperation(2113,pp::SessionOperation::Pause,pp::ClientContext::RaceDirectorSmug,Stage14CFixture::Owner); resumeLights->control.tick(resumeInitialGo+2000000); resumeLights->engine.tick(); resumeLights->settle(resumeInitialGo+2010000); resumeLights->browser.submitOperation(2114,pp::SessionOperation::Resume,pp::ClientContext::RaceDirectorSmug,Stage14CFixture::Owner); const pp::Time resumeRequest2=resumeInitialGo+4000000; resumeLights->control.tick(resumeRequest2); resumeLights->engine.tick(); const pp::Time resumeGo=resumeLights->control.scheduledGo(); const pp::Time resumeFirst=resumeGo-pp::Time(resumeLights->control.redLightCount())*resumeLights->control.definition()->redIntervalUs()-resumeLights->control.finalDelay(); resumeLights->control.tick(resumeFirst); const auto resumeOne=resumeLights->noticeboard.currentAt(resumeFirst); resumeLights->control.tick(resumeFirst+resumeLights->control.definition()->redIntervalUs()); const auto resumeTwo=resumeLights->noticeboard.currentAt(resumeFirst+resumeLights->control.definition()->redIntervalUs()); resumeLights->control.tick(resumeGo); const auto resumeGoState=resumeLights->noticeboard.currentAt(resumeGo); const bool resumeSequence=resumeSubmitted&&resumeOne.startPhase==pp::StartPresentationPhase::ResumeCountdown&&resumeOne.redLightsLit==1&&resumeTwo.redLightsLit==2&&resumeGoState.startPhase==pp::StartPresentationPhase::Go&&resumeGoState.redLightsLit==0; s14c("14C.P10",resumeSequence,"authoritative_resume_steps=%u values=%u/%u/%u",resumeSequence?1:0,unsigned(resumeOne.redLightsLit),unsigned(resumeTwo.redLightsLit),unsigned(resumeGoState.redLightsLit)); resumeLights->~Stage14CFixture();
}
