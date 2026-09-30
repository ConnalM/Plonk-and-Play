#include <Arduino.h>
#include <Preferences.h>
#include "pp/system_time.h"
#include <stdarg.h>
#include "pp/core.h"
#include "pp/verification.h"
#ifndef PP_DIAGNOSTICS
#define PP_DIAGNOSTICS 1
#endif
#ifdef PP_ACCEPTANCE
uint64_t acceptanceReadSharedTime();
#endif
namespace {
using pp::systemTime;
// Development diagnostics: bounded queue, bounded flush, no wait for UART.
class Diagnostics {
public:
  void log(const char* format,...) {
    if(!enabled)return;
    if(size==Capacity){++dropped;return;}
    va_list args;va_start(args,format);vsnprintf(lines[(head+size)%Capacity],Width,format,args);va_end(args);++size;
  }
  void flush(){
    if(!size)return;
    const auto length=strlen(lines[head]);const int available=Serial.availableForWrite();
    if(available<=0)return;
    if(offset<length){size_t n=length-offset;if(n>size_t(available))n=available;Serial.write(reinterpret_cast<uint8_t*>(lines[head]+offset),n);offset+=n;}
    else {Serial.write('\n');head=(head+1)%Capacity;--size;offset=0;}
  }
  void enable(bool value){enabled=value;if(!value){size=0;head=0;offset=0;}}
  bool enabled=PP_DIAGNOSTICS;uint32_t dropped=0;
private:
  static constexpr size_t Capacity=64,Width=160;
  char lines[Capacity][Width]{};size_t head=0,size=0,offset=0;
} diagnostics;
class NvsStore final:public pp::Store {
public:
#ifdef PP_ACCEPTANCE
  uint32_t readCalls=0;
#endif
  void begin(const char* name="pp-stage1"){ready=preferences.begin(name,false);}
  bool available()const override{return ready;}
  bool read(unsigned s,uint8_t* data,size_t n)override{
#ifdef PP_ACCEPTANCE
    ++readCalls;
#endif
    if(!ready)return false;
    const char* key=s?"config-b":"config-a";
    if(!preferences.isKey(key))return false;
    auto length=preferences.getBytesLength(key);
    if(!length)return false;
    // Preserve the distinction between absent and malformed records.
    if(length!=n){memset(data,0,n);return true;}
    return preferences.getBytes(key,data,n)==n;
  }
  bool write(unsigned s,const uint8_t* data,size_t n)override{return ready&&preferences.putBytes(s?"config-b":"config-a",data,n)==n;}
private: Preferences preferences;bool ready=false;
} storage;
pp::Bus bus;
pp::Memory memory(storage);
const auto lifecycle=bus.attach(pp::Role::Lifecycle);
const auto memoryEndpoint=bus.attach(pp::Role::Memory);
pp::InputModule input(bus,bus.attach(pp::Role::Input));
pp::RaceControl raceControl{bus.attach(pp::Role::RaceControl)};
pp::RaceEngine raceEngine{bus.attach(pp::Role::RaceEngine)};
pp::OutputModule output{bus.attach(pp::Role::Output)};
pp::Presentation presentation{bus.attach(pp::Role::Presentation)};
const auto testPublisher=bus.attach(pp::Role::Diagnostics);
const auto testObserver=bus.attach(pp::Role::Diagnostics);
const auto inputObserver=bus.attach(pp::Role::Diagnostics);
pp::MemoryModule memoryModule(bus,memoryEndpoint,memory);
pp::Configuration workingConfiguration;
bool ready=false,testsPassed=true,bootFailed=false;
pp::Time nextStatus=0;
#ifdef PP_VERIFY
bool verificationReboot=false;
pp::Time rebootAt=0;
#endif
void report(const char* name,bool passed){diagnostics.log("[TEST] %s %s",passed?"PASS":"FAIL",name);}
void observeInputEvents() {
  pp::Message event;
  while(bus.receive(inputObserver,event)) {
    diagnostics.log("[INPUT EVENT] device=%08lx capability=%u relevant_us=%llu",
      static_cast<unsigned long>(event.input.device),event.input.capability,
      static_cast<unsigned long long>(event.relevantTime));
  }
}
void status(){diagnostics.log("[DEV] Stage1 %s system_us=%llu dropped=%lu; no session, no race",
  ready?"IDLE":bootFailed?"FAULT":"STARTING",static_cast<unsigned long long>(systemTime()),static_cast<unsigned long>(diagnostics.dropped));}
#ifdef PP_ACCEPTANCE
#include "../tests/acceptance_probe.inc"
#endif
#ifdef PP_STAGE2_ACCEPTANCE
#include "../tests/stage2_acceptance_probe.inc"
#endif
#ifdef PP_STAGE3_ACCEPTANCE
#include "../tests/stage3_acceptance_probe.inc"
#endif
}
void setup(){
  Serial.begin(115200);
#ifdef PP_ACCEPTANCE
  acceptanceBeforeBoot();
#endif
#ifdef PP_STAGE2_ACCEPTANCE
  stage2AcceptanceBeforeBoot();
#endif
#ifdef PP_STAGE3_ACCEPTANCE
  stage3AcceptanceBeforeBoot();
#endif
  diagnostics.log("[DEV] P&P STAGE 1 -- diagnostics are not product State");
  auto first=systemTime(),second=systemTime();testsPassed=second>=first;
  diagnostics.log("[INIT] System Time %s: monotonic 64-bit microseconds",testsPassed?"READY":"FAIL");
  storage.begin();diagnostics.log("[INIT] Memory %s: NVS behind Memory boundary",storage.available()?"READY":"DEGRADED");
  testsPassed&=bus.subscribe(memoryEndpoint,pp::Type::LoadConfiguration);
  testsPassed&=bus.subscribe(lifecycle,pp::Type::ConfigurationLoaded);
  testsPassed&=bus.subscribe(testPublisher,pp::Type::DiagnosticProbe);
  testsPassed&=bus.subscribe(testObserver,pp::Type::DiagnosticProbe);
  testsPassed&=bus.subscribe(inputObserver,pp::Type::InputEvent);
  diagnostics.log("[INIT] Message Bus READY: bounded mailboxes; authority checked");
  testsPassed&=pp::busSelfTest(bus,testPublisher,testObserver,input.endpoint,report);
  diagnostics.log("[BUS SELF-TEST] %s",testsPassed?"PASS":"FAIL");
#ifdef PP_VERIFY
  testsPassed&=pp::memoryTests(report);
  // Exercise the actual Memory codec/adapter across an ESP32 software reset.
  // Only this verification image writes these disposable namespaces.
  Preferences marker;bool nvsOk=marker.begin("pp-test-phase",false);
  if(nvsOk){
    const bool secondBoot=marker.getBool("pending",false);
    NvsStore testStorage;testStorage.begin("pp-selftest");
    pp::Memory testMemory(testStorage);pp::Configuration saved;
    const auto result=testMemory.load(saved);
    if(secondBoot){
      nvsOk=result==pp::LoadStatus::Remembered&&saved.laps==27;
      nvsOk&=marker.putBool("pending",false)==1;
      report("nvs.configuration.survives.reset",nvsOk);
    }else{
      saved.laps=27;nvsOk=testMemory.save(saved);
      nvsOk&=marker.putBool("pending",true)==1;
      verificationReboot=nvsOk;rebootAt=systemTime()+2000000;
      report("nvs.configuration.saved.before.reset",nvsOk);
    }
    marker.end();
  }
  testsPassed&=nvsOk;
#endif
  pp::Message request;request.type=pp::Type::LoadConfiguration;request.correlation=1;
  if(bus.publish(lifecycle,request)!=pp::Delivery::Delivered){bootFailed=true;diagnostics.log("STAGE1_FAIL configuration request");}
}
void loop(){
  memoryModule.tick();
  input.tick(systemTime());
  observeInputEvents();
  pp::Message message;
  if(!ready&&!bootFailed&&bus.receive(lifecycle,message)){
    if(message.type!=pp::Type::ConfigurationLoaded||message.correlation!=1||!pp::valid(message.configuration)){
      bootFailed=true;diagnostics.log("STAGE1_FAIL configuration response");
    }else{
      workingConfiguration=message.configuration;
#ifdef PP_ACCEPTANCE
      acceptanceLoadStatus=unsigned(message.loadStatus);
#endif
      const char* sources[]={"remembered","factory: no saved configuration","factory: invalid saved configuration","factory: storage unavailable"};
      diagnostics.log("[INIT] Working configuration READY in RAM (%s)",sources[unsigned(message.loadStatus)]);
      diagnostics.log("[CONFIG] Lap Race proposed; lanes=%u laps=%lu optional_features=off",workingConfiguration.lanes,static_cast<unsigned long>(workingConfiguration.laps));
      diagnostics.log("[INIT] Input Module READY: one simulated detector; Race Control / Race Engine / Output / Presentation dormant");
      diagnostics.log("[INIT] No capabilities discovered or assigned; session readiness not claimed");
      ready=testsPassed;
      bootFailed=!testsPassed;
#ifdef PP_VERIFY
      if(verificationReboot)diagnostics.log("[TEST] Rebooting after diagnostics drain to verify persistent configuration");
      else
#endif
      diagnostics.log("%s",ready?"STAGE1_PASS skeleton initialised; no race behaviour":"STAGE1_FAIL self-test");
      diagnostics.log("%s",ready?"STAGE2_PASS one simulated Input Device ready; no race behaviour":"STAGE2_FAIL startup");
      diagnostics.log("%s",ready?"STAGE3_PASS Message Bus delivery boundary ready; no race behaviour":"STAGE3_FAIL startup");
      diagnostics.log("[DEV] Serial: ? status, t bus self-test, q quiet, v diagnostics on");
      nextStatus=systemTime()+10000000;
    }
  }
  // Bounded serial command work. 0/1 are a development-only simulated source,
  // exercised through the Input Device; they are not P&P Requests or bus events.
  for(unsigned i=0;i<8&&Serial.available();++i){
    const char c=Serial.read();
    if(c=='q')diagnostics.enable(false);
    else if(c=='v'){diagnostics.enable(true);status();}
    else if(c=='?')status();
    else if(c=='t'){const bool ok=pp::busSelfTest(bus,testPublisher,testObserver,input.endpoint,report);diagnostics.log("[BUS SELF-TEST] %s",ok?"PASS":"FAIL");}
    else if(c=='0'){input.setSimulatedSource(false);diagnostics.log("[INPUT SOURCE] simulated detector INACTIVE");}
    else if(c=='1'){input.setSimulatedSource(true);diagnostics.log("[INPUT SOURCE] simulated detector ACTIVE");}
#ifdef PP_ACCEPTANCE
    else acceptanceCommand(c);
#endif
#ifdef PP_STAGE2_ACCEPTANCE
    else stage2AcceptanceCommand(c);
#endif
#ifdef PP_STAGE3_ACCEPTANCE
    else stage3AcceptanceCommand(c);
#endif
  }
  if(ready&&systemTime()>=nextStatus){status();nextStatus=systemTime()+10000000;}
  diagnostics.flush();delay(1);
#ifdef PP_ACCEPTANCE
  acceptanceTick();
#endif
#ifdef PP_STAGE2_ACCEPTANCE
  stage2AcceptanceTick();
#endif
#ifdef PP_STAGE3_ACCEPTANCE
  stage3AcceptanceTick();
#endif
#ifdef PP_VERIFY
  if(verificationReboot&&systemTime()>=rebootAt)ESP.restart();
#endif
}
