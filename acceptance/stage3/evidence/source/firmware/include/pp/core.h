#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace pp {
using Time = uint64_t; // Monotonic microseconds in this controller boot's domain.
enum class Role : uint8_t { Lifecycle, Memory, Input, RaceControl, RaceEngine, Output, Presentation, Diagnostics };
constexpr uint16_t mask(Role r) { return uint16_t(1u << unsigned(r)); }
// Internal Stage 1 contracts. No race messages or invented input events.
enum class Type : uint8_t { LoadConfiguration, ConfigurationLoaded, InputEvent, DiagnosticProbe, Count };
struct InputIdentity {
  uint32_t device;
  uint16_t capability;
  constexpr InputIdentity(uint32_t deviceValue=0, uint16_t capabilityValue=0):device(deviceValue),capability(capabilityValue) {}
};
struct Configuration {
  uint16_t lanes = 2;
  uint32_t laps = 10;
  uint32_t optionalFeatures = 0;
  bool soundIfAvailable = true;
  bool powerIfAvailable = true;
  // Only Lap Race is implemented as proposed data, not executable race logic.
  uint8_t mode = 1;
};
inline bool valid(const Configuration& c) {
  return c.lanes > 0 && c.laps > 0 && c.mode == 1 && c.optionalFeatures == 0;
}
enum class LoadStatus : uint8_t { Remembered, DefaultsMissing, DefaultsInvalid, DefaultsStorageError };
struct Message {
  Type type = Type::DiagnosticProbe;
  uint16_t source = 0; // Set by bus, never trusted from a caller.
  Time relevantTime = 0;
  uint32_t correlation = 0;
  uint32_t eventId = 0;
  InputIdentity input{};
  Configuration configuration{};
  LoadStatus loadStatus = LoadStatus::DefaultsMissing;
  uint32_t probe = 0;
};
enum class Delivery { Delivered, Forbidden, Invalid, NoSubscribers, Full };
class Bus {
public:
  static constexpr size_t Participants = 12, Depth = 8;
  struct Endpoint { uint16_t id; Endpoint(uint16_t value=0):id(value) {} };
  Endpoint attach(Role role) {
    if (count_ == Participants) return {};
    slots_[count_].role = role;
    return {uint16_t(++count_)};
  }
  bool subscribe(Endpoint e, Type t) {
    auto* s = slot(e);
    if (!s || unsigned(t) >= unsigned(Type::Count) || !(consumers(t) & mask(s->role))) return false;
    s->subscriptions |= uint16_t(1u << unsigned(t)); return true;
  }
  bool unsubscribe(Endpoint e, Type t) {
    auto* s = slot(e);
    if (!s || unsigned(t) >= unsigned(Type::Count) || !(consumers(t) & mask(s->role))) return false;
    s->subscriptions &= uint16_t(~(1u << unsigned(t))); return true;
  }
  Delivery publish(Endpoint e, Message m) {
    auto* sender = slot(e);
    if (!sender || unsigned(m.type) >= unsigned(Type::Count)) return Delivery::Invalid;
    if (!(publishers(m.type) & mask(sender->role))) return Delivery::Forbidden;
    if ((m.type == Type::LoadConfiguration || m.type == Type::ConfigurationLoaded) && m.correlation == 0) return Delivery::Invalid;
    size_t recipients = 0;
    for (size_t i=0;i<count_;++i) if (interested(slots_[i], m.type)) {
      ++recipients;
      if (slots_[i].size == Depth) return Delivery::Full;
    }
    if (!recipients) return Delivery::NoSubscribers;
    m.source = e.id;
    // Atomic fan-out: explicit failure, never a partially delivered publication.
    for (size_t i=0;i<count_;++i) if (interested(slots_[i], m.type)) {
      auto& s=slots_[i]; s.queue[(s.head+s.size)%Depth]=m; ++s.size;
    }
    return Delivery::Delivered;
  }
  bool receive(Endpoint e, Message& out) {
    auto* s=slot(e); if (!s || !s->size) return false;
    out=s->queue[s->head]; s->head=(s->head+1)%Depth; --s->size; return true;
  }
private:
  struct Slot { Role role=Role::Diagnostics; uint16_t subscriptions=0; Message queue[Depth]{}; size_t head=0,size=0; };
  Slot slots_[Participants]{}; size_t count_=0;
  Slot* slot(Endpoint e) { return e.id && e.id<=count_ ? &slots_[e.id-1] : nullptr; }
  static bool interested(const Slot& s, Type t) { return s.subscriptions & (1u << unsigned(t)); }
  static uint16_t publishers(Type t) {
    switch(t) {
      case Type::LoadConfiguration: return mask(Role::Lifecycle);
      case Type::ConfigurationLoaded: return mask(Role::Memory);
      case Type::InputEvent: return mask(Role::Input);
      case Type::DiagnosticProbe: return mask(Role::Diagnostics);
      default: return 0;
    }
  }
  static uint16_t consumers(Type t) {
    switch(t) {
      case Type::LoadConfiguration: return mask(Role::Memory);
      case Type::ConfigurationLoaded: return mask(Role::Lifecycle);
      case Type::InputEvent: return mask(Role::RaceEngine)|mask(Role::Diagnostics);
      case Type::DiagnosticProbe: return mask(Role::Diagnostics);
      default: return 0;
    }
  }
};

// Storage technology remains behind Memory. False read means absent or invalid;
// available() distinguishes a failed storage service from first commissioning.
class Store {
public:
  virtual bool available() const = 0;
  virtual bool read(unsigned slot, uint8_t* data, size_t size) = 0;
  virtual bool write(unsigned slot, const uint8_t* data, size_t size) = 0;
  virtual ~Store() = default;
};
class Memory {
public:
  static constexpr size_t RecordSize=32;
  explicit Memory(Store& s):store_(s) {}
  LoadStatus load(Configuration& c) {
    c=Configuration{};
    if (!store_.available()) return LoadStatus::DefaultsStorageError;
    uint8_t a[RecordSize]{},b[RecordSize]{};
    const bool hasA=store_.read(0,a,sizeof(a)), hasB=store_.read(1,b,sizeof(b));
    Configuration ca,cb; uint32_t ga=0,gb=0;
    const bool va=hasA&&decode(a,ca,ga), vb=hasB&&decode(b,cb,gb);
    if (!va&&!vb) return hasA||hasB ? LoadStatus::DefaultsInvalid : LoadStatus::DefaultsMissing;
    const bool useB=vb&&(!va || gb>ga);
    c=useB?cb:ca; generation_=useB?gb:ga; active_=useB?1:0;
    return LoadStatus::Remembered;
  }
  bool save(const Configuration& c) {
    if (!store_.available() || !valid(c) || generation_==UINT32_MAX) return false;
    uint8_t bytes[RecordSize]{}; encode(c,generation_+1,bytes);
    const unsigned target=1-active_;
    if (!store_.write(target,bytes,sizeof(bytes))) return false;
    uint8_t verify[RecordSize]{};
    if (!store_.read(target,verify,sizeof(verify)) || memcmp(bytes,verify,sizeof(bytes))) return false;
    active_=target; ++generation_; return true;
  }
  static void encode(const Configuration& c,uint32_t generation,uint8_t* p) {
    memset(p,0,RecordSize); put(p,0x31505043); put(p+4,generation);
    put(p+8,c.lanes); put(p+12,c.laps); put(p+16,c.optionalFeatures);
    p[20]=c.soundIfAvailable; p[21]=c.powerIfAvailable; p[22]=c.mode;
    put(p+28,checksum(p,28));
  }
private:
  Store& store_; uint32_t generation_=0; unsigned active_=1;
  static void put(uint8_t* p,uint32_t v) { for(unsigned i=0;i<4;++i)p[i]=uint8_t(v>>(8*i)); }
  static uint32_t get(const uint8_t* p) { uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(p[i])<<(8*i);return v; }
  static uint32_t checksum(const uint8_t* p,size_t n) { uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h; }
  static bool decode(const uint8_t* p,Configuration& c,uint32_t& generation) {
    if(get(p)!=0x31505043 || get(p+28)!=checksum(p,28) || get(p+8)>UINT16_MAX || p[20]>1 || p[21]>1) return false;
    c.lanes=get(p+8);c.laps=get(p+12);c.optionalFeatures=get(p+16);
    c.soundIfAvailable=p[20];c.powerIfAvailable=p[21];c.mode=p[22];generation=get(p+4);
    return generation>0&&valid(c);
  }
};

// The simulated detector is one Input Device within the Input Module. Its native
// source state, filtering and re-arm state stay here: they never become bus data.
class SimulatedDetector {
public:
  static constexpr Time DetectionStableUs = 20000;
  static constexpr Time ClearStableUs = 20000;
  explicit SimulatedDetector(InputIdentity identity):identity_(identity) {}
  // Returns a clean trigger only after the source has been stable long enough.
  // The returned time is when recognition became true, rather than later polling.
  bool sample(bool active, Time sampleTime, Time& triggerTime) {
    if (active != sourceActive_) { sourceActive_=active; stableSince_=sampleTime; }
    if (armed_) {
      if (sourceActive_ && sampleTime-stableSince_>=DetectionStableUs) {
        armed_=false; triggerTime=stableSince_+DetectionStableUs; return true;
      }
    } else if (!sourceActive_ && sampleTime-stableSince_>=ClearStableUs) {
      armed_=true;
    }
    return false;
  }
  InputIdentity identity() const { return identity_; }
private:
  InputIdentity identity_;
  bool sourceActive_=false;
  bool armed_=true;
  Time stableSince_=0;
};

class InputModule {
public:
  static constexpr InputIdentity simulatedDetectorIdentity() { return InputIdentity(0x50500001u, 1); }
  InputModule(Bus& bus, Bus::Endpoint endpoint):endpoint(endpoint),bus_(bus),detector_(simulatedDetectorIdentity()) {}
  Bus::Endpoint endpoint;
  // This is the source-facing simulated-device boundary, not a bus injection API.
  void setSimulatedSource(bool active) { sourceActive_=active; }
  void tick(Time now) { sampleSource(sourceActive_,now); }
  void sampleSource(bool active, Time observedAt) {
    Time triggerAt=0;
    if (!detector_.sample(active,observedAt,triggerAt)) return;
    Message event{}; event.type=Type::InputEvent; event.input=detector_.identity(); event.relevantTime=triggerAt;
    lastDelivery_=bus_.publish(endpoint,event); ++cleanTriggers_;
  }
  uint32_t cleanTriggers() const { return cleanTriggers_; }
  Delivery lastDelivery() const { return lastDelivery_; }
private:
  Bus& bus_;
  SimulatedDetector detector_;
  bool sourceActive_=false;
  uint32_t cleanTriggers_=0;
  Delivery lastDelivery_=Delivery::NoSubscribers;
};

// No Registry discoveries, session, sensor assignment or race readiness is invented.
struct RaceControl { Bus::Endpoint endpoint; };
struct RaceEngine { Bus::Endpoint endpoint; };
struct OutputModule { Bus::Endpoint endpoint; };
struct Presentation { Bus::Endpoint endpoint; };

class MemoryModule {
public:
  MemoryModule(Bus& bus,Bus::Endpoint endpoint,Memory& memory):bus_(bus),endpoint_(endpoint),memory_(memory) {}
  void tick() {
    if (pending_) { if(bus_.publish(endpoint_,response_)==Delivery::Delivered)pending_=false;return; }
    Message request;
    if(!bus_.receive(endpoint_,request))return;
    response_={}; response_.type=Type::ConfigurationLoaded;response_.correlation=request.correlation;
    response_.loadStatus=memory_.load(response_.configuration);
    pending_=true;
  }
private:
  Bus& bus_; Bus::Endpoint endpoint_; Memory& memory_; Message response_{}; bool pending_=false;
};
} // namespace pp
