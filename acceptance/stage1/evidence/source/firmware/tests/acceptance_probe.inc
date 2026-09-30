// Compiled ONLY with PP_ACCEPTANCE. External observer and fixture controls,
// not an alternative boot, configuration loader, bus or product interface.
Diagnostics acceptanceOutput;
unsigned acceptancePhase=0,acceptanceLoadStatus=99,acceptanceSnapshots=0;
pp::Time acceptanceRestartAt=0;
pp::Bus::Endpoint acceptanceUnsubscribed;
void acceptanceBeforeBoot() {
  Preferences marker;
  if(marker.begin("pp-acceptance",false)) {
    acceptancePhase=marker.getUInt("phase",0);marker.end();
  }
  if(acceptancePhase==12)diagnostics.enable(false);
  acceptanceOutput.enable(true);
  acceptanceUnsubscribed=bus.attach(pp::Role::Diagnostics);
}
void acceptanceSnapshot() {
  acceptanceOutput.log("ACC SNAP phase=%u sample=%u ready=%u fault=%u diag=%u time=%llu reads=%lu",
    acceptancePhase,++acceptanceSnapshots,ready,bootFailed,diagnostics.enabled,
    static_cast<unsigned long long>(acceptanceReadSharedTime()),static_cast<unsigned long>(storage.readCalls));
  acceptanceOutput.log("ACC CONFIG phase=%u status=%u lanes=%u laps=%lu features=%lu sound=%u power=%u mode=%u",
    acceptancePhase,acceptanceLoadStatus,workingConfiguration.lanes,
    static_cast<unsigned long>(workingConfiguration.laps),static_cast<unsigned long>(workingConfiguration.optionalFeatures),
    workingConfiguration.soundIfAvailable,workingConfiguration.powerIfAvailable,workingConfiguration.mode);
}
void acceptanceBus() {
  pp::Message sent;sent.probe=271828;sent.relevantTime=1234567890123ull;sent.eventId=73;sent.source=999;
  const auto delivery=bus.publish(testPublisher,sent);
  pp::Message a,b,n;
  const bool gotA=bus.receive(testPublisher,a),gotB=bus.receive(testObserver,b);
  const bool extraA=bus.receive(testPublisher,n),extraB=bus.receive(testObserver,n);
  const bool unrelated=bus.receive(acceptanceUnsubscribed,n);
  acceptanceOutput.log("ACC BUS phase=%u result=%u a=%u b=%u extraA=%u extraB=%u unrelated=%u publisher=%u",
    acceptancePhase,unsigned(delivery),gotA,gotB,extraA,extraB,unrelated,testPublisher.id);
  acceptanceOutput.log("ACC RECEIVED who=A type=%u source=%u token=%lu time=%llu id=%lu",
    unsigned(a.type),a.source,static_cast<unsigned long>(a.probe),static_cast<unsigned long long>(a.relevantTime),static_cast<unsigned long>(a.eventId));
  acceptanceOutput.log("ACC RECEIVED who=B type=%u source=%u token=%lu time=%llu id=%lu",
    unsigned(b.type),b.source,static_cast<unsigned long>(b.probe),static_cast<unsigned long long>(b.relevantTime),static_cast<unsigned long>(b.eventId));
  const auto rejected=bus.publish(input.endpoint,sent);
  const bool rejectedA=bus.receive(testPublisher,n),rejectedB=bus.receive(testObserver,n);
  acceptanceOutput.log("ACC AUTH result=%u receivedA=%u receivedB=%u",unsigned(rejected),rejectedA,rejectedB);
}
void acceptanceFixture(unsigned phase) {
  bool ok=true;
  if(phase==4) {
    auto fixture=workingConfiguration;fixture.laps=37;
    // Memory owns encoding/writes; this injects a saved fixture at its boundary.
    ok=memory.save(fixture);
  } else if(phase==5) {
    // Deliberately bypass encoding ONLY to corrupt storage at the storage seam.
    uint8_t garbage[pp::Memory::RecordSize];memset(garbage,0xa5,sizeof(garbage));
    ok=storage.write(0,garbage,sizeof(garbage));ok=storage.write(1,garbage,sizeof(garbage))&&ok;
  }
  Preferences marker;
  if(marker.begin("pp-acceptance",false)) {ok=(marker.putUInt("phase",phase)==4)&&ok;marker.end();} else ok=false;
  acceptanceOutput.log("ACC FIXTURE phase=%u ok=%u",phase,ok);
  if(ok)acceptanceRestartAt=systemTime()+250000;
}
void acceptanceCommand(char c) {
  if(c=='a')acceptanceSnapshot();
  else if(c=='b')acceptanceBus();
  else if(c=='r') {
    const auto before=storage.readCalls;volatile uint32_t sum=0;
    for(unsigned i=0;i<10000;++i)sum+=workingConfiguration.laps;
    acceptanceOutput.log("ACC RAM iterations=10000 sum=%lu readsBefore=%lu readsAfter=%lu",
      static_cast<unsigned long>(sum),static_cast<unsigned long>(before),static_cast<unsigned long>(storage.readCalls));
  }
  else if(c=='w')acceptanceFixture(4);
  else if(c=='x')acceptanceFixture(5);
  else if(c=='d')acceptanceFixture(12);
  else if(c=='z')acceptanceOutput.log("ACC DONE");
}
void acceptanceTick() {
  acceptanceOutput.flush();
  if(acceptanceRestartAt&&systemTime()>=acceptanceRestartAt)ESP.restart();
}
