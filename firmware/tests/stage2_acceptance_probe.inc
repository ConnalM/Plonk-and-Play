// Compiled only for the Stage 2 acceptance image. It drives the simulated
// detector through InputModule::sampleSource(), never by fabricating a bus event.
Diagnostics stage2Output;
pp::Bus::Endpoint stage2Observer;
unsigned stage2Phase=0;
pp::Time stage2RestartAt=0;
bool stage2ReadyReported=false;

void stage2AcceptanceBeforeBoot() {
  Preferences marker;
  if(marker.begin("pp-s2-acc",false)) {
    stage2Phase=marker.getUInt("phase",0); marker.end();
  }
  stage2Observer=bus.attach(pp::Role::Diagnostics);
  bus.subscribe(stage2Observer,pp::Type::InputEvent);
  // Keep captured acceptance lines atomic; normal diagnostics are tested by the
  // production image and must not interleave with this independent observer.
  diagnostics.enable(false);
  stage2Output.enable(true);
}

void stage2Drain(const char* test, unsigned expected, pp::Time observedAt) {
  pp::Message event; unsigned count=0; pp::Message first{};
  while(bus.receive(stage2Observer,event)) { if(count++==0) first=event; }
  // The ordinary development observer is not the subject of this dense fixture.
  // Drain it here so its bounded mailbox cannot make a later source sample fail.
  while(bus.receive(inputObserver,event)) {}
  stage2Output.log("ACC INPUT test=%s count=%u expected=%u device=%08lx capability=%u relevant=%llu observed=%llu",
    test,count,expected,static_cast<unsigned long>(first.input.device),first.input.capability,
    static_cast<unsigned long long>(first.relevantTime),static_cast<unsigned long long>(observedAt));
}

void stage2Sample(bool active, pp::Time at) { input.sampleSource(active,at); }
void stage2Rearm(pp::Time start) {
  stage2Sample(false,start); stage2Sample(false,start+pp::SimulatedDetector::ClearStableUs);
}

void stage2RunFirstBoot() {
  // 2.1: one clean activation and a normal clear.
  stage2Rearm(10000); stage2Sample(true,100000); stage2Sample(true,120000);
  stage2Sample(false,121000); stage2Sample(false,141000); stage2Drain("2.1",1,150000);

  // 2.2: staying ACTIVE cannot generate a second event.
  stage2Sample(true,200000); stage2Sample(true,220000); stage2Sample(true,300000);
  stage2Sample(false,301000); stage2Sample(false,321000); stage2Drain("2.2",1,330000);

  // 2.3: a valid clear re-arms and permits a second clean activation.
  stage2Sample(true,400000); stage2Sample(true,420000); stage2Sample(false,421000); stage2Sample(false,441000);
  stage2Sample(true,450000); stage2Sample(true,470000); stage2Sample(false,471000); stage2Sample(false,491000);
  stage2Drain("2.3",2,500000);

  // 2.4: short ACTIVE chatter is rejected before the later valid trigger.
  stage2Sample(true,520000); stage2Sample(false,530000); stage2Sample(true,540000); stage2Sample(true,560000);
  stage2Sample(false,561000); stage2Sample(false,581000); stage2Drain("2.4",1,590000);

  // 2.5: short inactive chatter after a trigger must not re-arm the detector.
  stage2Sample(true,600000); stage2Sample(true,620000); stage2Sample(false,621000); stage2Sample(true,630000);
  stage2Sample(true,650000); stage2Sample(false,660000); stage2Sample(false,680000);
  stage2Sample(true,690000); stage2Sample(true,710000); stage2Sample(false,711000); stage2Sample(false,731000);
  stage2Drain("2.5",2,740000);

  // 2.6: recognition happens at 820000; the queued event is observed later.
  stage2Sample(true,800000); stage2Sample(true,820000); stage2Sample(false,821000); stage2Sample(false,841000);
  stage2Drain("2.6",1,900000);

  // First half of 2.7; phase two repeats after an actual ESP restart.
  stage2Sample(true,920000); stage2Sample(true,940000); stage2Sample(false,941000); stage2Sample(false,961000);
  stage2Drain("2.7a",1,970000);
  const auto identity=pp::InputModule::simulatedDetectorIdentity();
  stage2Output.log("ACC DEVICE phase=0 device=%08lx capability=%u",static_cast<unsigned long>(identity.device),identity.capability);
}

void stage2RestartForIdentity() {
  Preferences marker; bool ok=marker.begin("pp-s2-acc",false);
  if(ok) { ok=marker.putUInt("phase",1)==4; marker.end(); }
  stage2Output.log("ACC FIXTURE identity_restart=%u",ok);
  if(ok) stage2RestartAt=systemTime()+250000;
}

void stage2RunSecondBoot() {
  stage2Rearm(10000); stage2Sample(true,100000); stage2Sample(true,120000); stage2Sample(false,121000); stage2Sample(false,141000);
  stage2Drain("2.7b",1,150000);
  const auto identity=pp::InputModule::simulatedDetectorIdentity();
  stage2Output.log("ACC DEVICE phase=1 device=%08lx capability=%u",static_cast<unsigned long>(identity.device),identity.capability);
  stage2Output.log("ACC DONE");
}

void stage2AcceptanceCommand(char c) {
  if(c=='s' && stage2Phase==0) stage2RunFirstBoot();
  else if(c=='i' && stage2Phase==0) stage2RestartForIdentity();
  else if(c=='j' && stage2Phase==1) stage2RunSecondBoot();
}
void stage2AcceptanceTick() {
  stage2Output.flush();
  if(ready&&!stage2ReadyReported) { stage2ReadyReported=true; stage2Output.log("ACC READY phase=%u",stage2Phase); }
  if(stage2RestartAt&&systemTime()>=stage2RestartAt) ESP.restart();
}
