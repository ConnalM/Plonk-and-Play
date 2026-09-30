#pragma once
#include "core.h"
namespace pp {
using Report = void (*)(const char*,bool);
// Exercises the SAME bus instance used by Memory and lifecycle. Probe messages
// are authorised Diagnostics-only messages, never counterfeit race/input facts.
inline bool busSelfTest(Bus& bus,Bus::Endpoint producer,Bus::Endpoint observer,Bus::Endpoint forbidden,Report report) {
  bool passed=true; auto check=[&](const char* n,bool ok){report(n,ok);passed&=ok;};
  Message m{}; m.probe=0x504e50;m.relevantTime=1234567890123ull;m.eventId=7;m.source=999;
  Message a{},b{};
  check("bus.authority.publish",bus.publish(forbidden,m)==Delivery::Forbidden);
  check("bus.authority.subscribe",!bus.subscribe(forbidden,Type::DiagnosticProbe));
  check("bus.fanout.publish",bus.publish(producer,m)==Delivery::Delivered);
  check("bus.fanout.identical",bus.receive(producer,a)&&bus.receive(observer,b)&&a.probe==m.probe&&b.probe==m.probe&&a.source==producer.id&&b.source==producer.id&&a.relevantTime==m.relevantTime&&b.eventId==7);
  check("bus.no.duplicate",!bus.receive(producer,a)&&!bus.receive(observer,b));
  for(unsigned i=0;i<Bus::Depth;++i){m.probe=i;passed&=bus.publish(producer,m)==Delivery::Delivered;}
  for(unsigned i=0;i<Bus::Depth;++i)passed&=bus.receive(producer,a)&&a.probe==i;
  check("bus.backpressure.atomic",bus.publish(producer,m)==Delivery::Full&&!bus.receive(producer,a));
  bool ordered=true;for(unsigned i=0;i<Bus::Depth;++i)ordered&=bus.receive(observer,b)&&b.probe==i;
  check("bus.fifo",ordered);
  check("bus.recovery",bus.publish(producer,m)==Delivery::Delivered&&bus.receive(producer,a)&&bus.receive(observer,b));
  return passed;
}
class FakeStore : public Store {
public:
  bool online=true,failWrite=false,torn=false,present[2]{};uint8_t data[2][Memory::RecordSize]{};
  bool available() const override {return online;}
  bool read(unsigned s,uint8_t* out,size_t n) override {if(!present[s])return false;memcpy(out,data[s],n);return true;}
  bool write(unsigned s,const uint8_t* in,size_t n) override {
    if(failWrite)return false;
    present[s]=true;memcpy(data[s],in,torn?n/2:n);return !torn;
  }
};
inline bool memoryTests(Report report) {
  bool all=true;auto check=[&](const char* n,bool ok){report(n,ok);all&=ok;};
  FakeStore store;Memory memory(store);Configuration c;
  check("memory.first.boot",memory.load(c)==LoadStatus::DefaultsMissing&&c.lanes==2&&c.laps==10&&c.optionalFeatures==0&&c.soundIfAvailable&&c.powerIfAvailable);
  c.laps=27;check("memory.save",memory.save(c));
  Memory reboot(store);Configuration loaded;check("memory.restore",reboot.load(loaded)==LoadStatus::Remembered&&loaded.laps==27);
  loaded.laps=99;check("memory.ram.isolation",reboot.load(c)==LoadStatus::Remembered&&c.laps==27);
  c.laps=31;store.failWrite=true;check("memory.write.failure",!reboot.save(c));store.failWrite=false;
  store.torn=true;check("memory.torn.write",!reboot.save(c));store.torn=false;
  Memory recovery(store);check("memory.last.good",recovery.load(c)==LoadStatus::Remembered&&c.laps==27);
  c.laps=31;check("memory.new.generation",recovery.save(c));Memory again(store);
  check("memory.newest",again.load(c)==LoadStatus::Remembered&&c.laps==31);
  store.data[1][12]^=1;Memory corruptNewest(store);
  check("memory.corrupt.fallback",corruptNewest.load(c)==LoadStatus::Remembered&&c.laps==27);
  store.data[0][0]=0;Memory corruptBoth(store);
  check("memory.corrupt.defaults",corruptBoth.load(c)==LoadStatus::DefaultsInvalid&&c.laps==10);
  c.lanes=0;check("memory.invalid.rejected",!corruptBoth.save(c));store.online=false;
  check("memory.unavailable",corruptBoth.load(c)==LoadStatus::DefaultsStorageError&&!corruptBoth.save(c));
  return all;
}
}
