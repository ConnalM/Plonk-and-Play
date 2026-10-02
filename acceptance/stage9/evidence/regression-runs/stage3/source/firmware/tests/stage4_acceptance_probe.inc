// Stage 4 test/session-preparation scaffolding only. It is not Race Control,
// START handling, a bus participant, or a production session-creation path.
Diagnostics stage4Output;
pp::Bus::Endpoint stage4Observer;
bool stage4ReadyReported=false;
pp::SessionInputRole stage4Proposed{pp::InputModule::simulatedDetectorIdentity(),1,pp::InputRole::StartFinish};
pp::SessionDefinition stage4First(stage4Proposed);

void stage4AcceptanceBeforeBoot() {
  stage4Observer=bus.attach(pp::Role::Diagnostics);bus.subscribe(stage4Observer,pp::Type::InputEvent);
  diagnostics.enable(false);stage4Output.enable(true);
}
void stage4Detection(pp::Time start) {
  input.sampleSource(false,start);input.sampleSource(false,start+20000);
  input.sampleSource(true,start+100000);input.sampleSource(true,start+120000);
  input.sampleSource(false,start+121000);input.sampleSource(false,start+141000);
}
void stage4Log(const char* test,const pp::SessionDefinition& definition,pp::Time observed) {
  pp::Message event{};unsigned count=0;while(bus.receive(stage4Observer,event)){++count;}while(bus.receive(inputObserver,event)){}
  pp::SessionInputRole role{};const bool mapped=definition.resolve(event.input,role);
  stage4Output.log("ACC S4 test=%s events=%u source=%u device=%08lx capability=%u relevant=%llu observed=%llu mapped=%u lane=%u role=%u",
    test,count,event.source,static_cast<unsigned long>(event.input.device),event.input.capability,
    static_cast<unsigned long long>(event.relevantTime),static_cast<unsigned long long>(observed),mapped,role.lane,unsigned(role.role));
}
void stage4Run() {
  stage4Detection(0);stage4Log("4.1",stage4First,150000);
  stage4Detection(200000);stage4Log("4.2",stage4First,350000);
  stage4Detection(400000);stage4Log("4.3",stage4First,550000);
  stage4Proposed.lane=2; // mutable proposal changes, fixed first session does not.
  stage4Detection(600000);stage4Log("4.4",stage4First,750000);
  pp::SessionDefinition second(stage4Proposed);
  stage4Detection(800000);stage4Log("4.5",second,950000);
  stage4Output.log("ACC S4 FIRST lane=1 SECOND lane=2 proposed=%u",stage4Proposed.lane);
  stage4Output.log("ACC DONE");
}
void stage4AcceptanceCommand(char c){if(c=='s')stage4Run();}
void stage4AcceptanceTick(){stage4Output.flush();if(ready&&!stage4ReadyReported){stage4ReadyReported=true;stage4Output.log("ACC READY");}}
