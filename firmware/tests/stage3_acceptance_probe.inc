// Compiled only for Stage 3 acceptance. It observes the real Bus boundary and
// drives the established InputModule source-facing path; it never injects a
// successful INPUT_EVENT downstream of that module.
Diagnostics stage3Output;
pp::Bus::Endpoint stage3A;
pp::Bus::Endpoint stage3B;
bool stage3ReadyReported=false;

void stage3AcceptanceBeforeBoot() {
  stage3A=bus.attach(pp::Role::Diagnostics);
  stage3B=bus.attach(pp::Role::Diagnostics);
  bus.subscribe(stage3A,pp::Type::InputEvent);
  // B is deliberately connected but initially unsubscribed.
  diagnostics.enable(false);
  stage3Output.enable(true);
}

void stage3Drain(const char* test, unsigned expectedA, unsigned expectedB, pp::Time observedAt) {
  pp::Message a{},b{},event{}; unsigned countA=0,countB=0;
  while(bus.receive(stage3A,event)) { if(countA++==0)a=event; }
  while(bus.receive(stage3B,event)) { if(countB++==0)b=event; }
  while(bus.receive(inputObserver,event)) {}
  const pp::Message& first=countA?a:b;
  stage3Output.log("ACC DELIVERY test=%s delivery=%u a=%u b=%u expectedA=%u expectedB=%u source=%u device=%08lx capability=%u relevant=%llu observed=%llu",
    test,unsigned(input.lastDelivery()),countA,countB,expectedA,expectedB,first.source,
    static_cast<unsigned long>(first.input.device),first.input.capability,
    static_cast<unsigned long long>(first.relevantTime),static_cast<unsigned long long>(observedAt));
}

void stage3ValidSourceDetection(pp::Time start) {
  input.sampleSource(false,start); input.sampleSource(false,start+pp::SimulatedDetector::ClearStableUs);
  input.sampleSource(true,start+100000); input.sampleSource(true,start+120000);
  input.sampleSource(false,start+121000); input.sampleSource(false,start+141000);
}

void stage3RejectedPublish(const char* test, pp::Bus::Endpoint sender) {
  pp::Message forged{}; forged.type=pp::Type::InputEvent;
  forged.input=pp::InputModule::simulatedDetectorIdentity(); forged.relevantTime=987654;
  const auto result=bus.publish(sender,forged); pp::Message event;
  const bool a=bus.receive(stage3A,event),b=bus.receive(stage3B,event);
  while(bus.receive(inputObserver,event)) {}
  stage3Output.log("ACC AUTH test=%s result=%u a=%u b=%u sender=%u",test,unsigned(result),a,b,sender.id);
}

void stage3Run() {
  stage3ValidSourceDetection(0);       stage3Drain("3.1",1,0,150000);
  stage3ValidSourceDetection(200000);  stage3Drain("3.2",1,0,350000);
  stage3ValidSourceDetection(400000);  stage3Drain("3.3",1,0,550000);
  stage3ValidSourceDetection(600000);  stage3Drain("3.4",1,0,750000);
  const bool added=bus.subscribe(stage3B,pp::Type::InputEvent);
  stage3ValidSourceDetection(800000);  stage3Drain("3.5",1,1,950000);
  const bool removed=bus.unsubscribe(stage3B,pp::Type::InputEvent);
  stage3ValidSourceDetection(1000000); stage3Drain("3.6",1,0,1150000);
  stage3Output.log("ACC SUBSCRIBERS added=%u removed=%u",added,removed);
  bus.subscribe(stage3B,pp::Type::InputEvent);
  stage3RejectedPublish("3.7",stage3B);
  stage3RejectedPublish("3.8",stage3A);
  stage3ValidSourceDetection(1200000); stage3Drain("3.10",1,1,1350000);
  bus.unsubscribe(stage3B,pp::Type::InputEvent);
  stage3ValidSourceDetection(1400000); stage3Drain("3.11",1,0,1550000);
  stage3Output.log("ACC DONE");
}

void stage3AcceptanceCommand(char c) { if(c=='s')stage3Run(); }
void stage3AcceptanceTick() {
  stage3Output.flush();
  if(ready&&!stage3ReadyReported) { stage3ReadyReported=true;stage3Output.log("ACC READY"); }
}
