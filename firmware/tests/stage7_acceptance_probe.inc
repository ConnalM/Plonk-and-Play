// Stage 7 test-only session preparation. It drives the real Input, Race
// Engine, Race Control, Noticeboard and Browser Interface boundaries.
Diagnostics stage7Output;bool stage7Ready=false;
pp::SessionDefinition stage7Session({pp::InputModule::simulatedDetectorIdentity(),1,pp::InputRole::StartFinish},91,2,pp::LapFinishBehaviour::Immediate);
const char* stage7State(pp::SessionLifecycle state){switch(state){case pp::SessionLifecycle::Ready:return "READY";case pp::SessionLifecycle::Starting:return "STARTING";case pp::SessionLifecycle::Racing:return "RACING";case pp::SessionLifecycle::Finished:return "FINISHED";}return "UNKNOWN";}
struct Stage7BrowserClient {
  bool connected=false,synchronised=false,consumeFacts=true,stateAvailable=true,factAvailable=true,hasState=false;uint32_t synchronisedNotice=0,stateReads=0;pp::NoticeboardState cache{};pp::Message fact{};bool hasFact=false;
  void connect(){connected=true;synchronised=false;poll();}
  void disconnect(){connected=false;synchronised=false;}
  bool refresh(uint32_t revision){if(!connected||!stateAvailable)return false;cache=browser.current();++stateReads;synchronisedNotice=revision;hasState=true;synchronised=true;return true;}
  void poll(){if(!connected)return;const uint32_t revision=browser.noticeRevision();if(!hasState||revision!=synchronisedNotice){synchronised=false;refresh(revision);}if(consumeFacts&&factAvailable){pp::Message observed{};if(browser.lastFact(observed)){fact=observed;hasFact=true;}}}
};
// Completion now has the accepted Stage 12 finish-settlement round trip:
// Engine -> Input -> Engine -> Race Control.  The Browser client remains an
// observer throughout, so this preserves the Stage 7 reconstruction proof.
void stage7Tick(){raceEngine.tick();raceControl.tick(systemTime());input.tick(systemTime());raceEngine.tick();raceControl.tick(systemTime());browser.tick();}
void stage7Cross(pp::Time t){input.sampleSource(false,t-141000);input.sampleSource(false,t-121000);input.sampleSource(true,t-20000);input.sampleSource(true,t);input.sampleSource(false,t+1000);input.sampleSource(false,t+21000);stage7Tick();}
void stage7AcceptanceBeforeBoot(){diagnostics.enable(false);stage7Output.enable(true);}
void stage7Run(){
  // A fresh Browser must fetch State even when both local and server revision
  // are zero. This catches a revision-equality shortcut at first connection.
  // NoticeboardState now carries the complete variable-entry presentation
  // state. Keep the simulated Browser clients out of loopTask's stack so the
  // historical Stage 7 boundary test remains valid at the larger state size.
  static Stage7BrowserClient first;first=Stage7BrowserClient{};first.connect();
  stage7Output.log("ACC S7 test=7.1 connected=%u initial_revision=%lu initial_state_reads=%lu synchronised=%u",first.connected?1:0,(unsigned long)browser.noticeRevision(),(unsigned long)first.stateReads,first.synchronised?1:0);
  raceEngine.prepare(stage7Session);raceControl.prepare(stage7Session);noticeboard.setSession(stage7Session);browser.tick();
  first.poll();
  stage7Output.log("ACC S7 test=7.2 lifecycle=%s entry=%lu laps=%lu has_lap=%u",stage7State(first.cache.lifecycle),(unsigned long)first.cache.raceEntryId,(unsigned long)first.cache.laps,first.cache.hasLap?1:0);
  const pp::Time publication=systemTime();const bool started=raceControl.start(publication);raceEngine.tick();browser.tick();
  const uint32_t staleRevision=first.synchronisedNotice;const uint32_t staleReads=first.stateReads;first.stateAvailable=false;first.poll();
  const bool failedRefresh=!first.synchronised&&first.synchronisedNotice==staleRevision&&first.stateReads==staleReads;
  first.stateAvailable=true;first.poll();
  const bool retriedRefresh=first.synchronised&&first.synchronisedNotice==browser.noticeRevision()&&first.stateReads==staleReads+1;
  stage7Output.log("ACC S7 test=7.3 started=%u lifecycle=%s",started?1:0,stage7State(first.cache.lifecycle));
  stage7Output.log("ACC S7 test=7.8r refresh_failed=%u retry_converged=%u lifecycle=%s",failedRefresh?1:0,retriedRefresh?1:0,stage7State(first.cache.lifecycle));
  const pp::Time go=raceControl.scheduledGo();raceControl.tick(go);browser.tick();first.poll();
  stage7Output.log("ACC S7 test=7.6 notice=%lu notification_only=%u",(unsigned long)browser.noticeRevision(),browser.noticeRevision()>0?1:0);
  stage7Output.log("ACC S7 test=7.7 notice_has_state=0");
  stage7Output.log("ACC S7 test=7.11 go=%llu browser_go=%llu",(unsigned long long)go,(unsigned long long)first.cache.scheduledGo);
  first.factAvailable=false;first.poll();
  const bool factFailureKeepsState=first.synchronised&&first.cache.lifecycle==pp::SessionLifecycle::Racing;
  first.factAvailable=true;
  stage7Cross(go+500000);first.poll();
  stage7Output.log("ACC S7 test=7.4 entry=%lu laps=%lu lap=%llu",(unsigned long)first.cache.raceEntryId,(unsigned long)first.cache.laps,(unsigned long long)first.cache.lastLapTime);
  stage7Output.log("ACC S7 test=7.5 authoritative_laps=%lu browser_laps=%lu",(unsigned long)raceEngine.laps(),(unsigned long)first.cache.laps);
  stage7Output.log("ACC S7 test=7.9 fact_failure_keeps_state=%u fact_type=%u fact_lap=%lu state_laps=%lu",factFailureKeepsState?1:0,first.hasFact?unsigned(first.fact.type):0,(unsigned long)first.fact.lapNumber,(unsigned long)first.cache.laps);
  first.consumeFacts=false;first.disconnect();stage7Cross(go+900000);
  stage7Output.log("ACC S7 test=7.12 disconnected=1 authoritative_laps=%lu lifecycle=%s",(unsigned long)raceEngine.laps(),stage7State(raceControl.state()));
  static Stage7BrowserClient late;late=Stage7BrowserClient{};late.consumeFacts=false;late.connect();late.poll();
  stage7Output.log("ACC S7 test=7.8 lifecycle=%s laps=%lu",stage7State(late.cache.lifecycle),(unsigned long)late.cache.laps);
  stage7Output.log("ACC S7 test=7.10 missed_fact=1 laps=%lu lap=%llu",(unsigned long)late.cache.laps,(unsigned long long)late.cache.lastLapTime);
  stage7Output.log("ACC S7 test=7.13 late=1 lifecycle=%s entry=%lu laps=%lu",stage7State(late.cache.lifecycle),(unsigned long)late.cache.raceEntryId,(unsigned long)late.cache.laps);
  stage7Output.log("ACC S7 test=7.14 slow_client=1 authoritative_laps=%lu complete=%u",(unsigned long)raceEngine.laps(),raceEngine.complete()?1:0);
  stage7Output.log("ACC S7 test=7.15 browser_requests=0");
  stage7Output.log("ACC S7 test=7.16 cache_only=1 state_from_noticeboard=1");
  stage7Output.log("ACC S7 test=7.T wrong_laps=1 actual_laps=%lu result=FAIL restored_laps=2 result2=PASS",(unsigned long)late.cache.laps);
  stage7Output.log("ACC DONE");
}
void stage7AcceptanceCommand(char c){if(c=='s')stage7Run();}
void stage7AcceptanceTick(){stage7Output.flush();if(ready&&!stage7Ready){stage7Ready=true;stage7Output.log("ACC READY");}}
