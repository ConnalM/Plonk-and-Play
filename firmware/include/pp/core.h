#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifndef PP_MAX_ENTRIES
#define PP_MAX_ENTRIES 8
#endif
static_assert(PP_MAX_ENTRIES > 0 && PP_MAX_ENTRIES <= 255, "PP_MAX_ENTRIES must fit entry counts");

namespace pp {
using Time = uint64_t; // Monotonic microseconds in this controller boot's domain.
enum class Role : uint8_t { Lifecycle, Memory, Input, RaceControl, RaceEngine, Output, Presentation, Diagnostics };
constexpr uint16_t mask(Role r) { return uint16_t(1u << unsigned(r)); }
enum class SessionMode : uint8_t { None=0, LapRace=1, OpenPractice=2, Endurance=3 };
// Numeric values through DiagnosticProbe are the accepted Stage 7 compatibility baseline.
// Stage 14C appends EnduranceExpired; earlier values remain stable.
enum class Type : uint8_t { LoadConfiguration=0, ConfigurationLoaded=1, InputEvent=2, GoScheduled=3, LapCompleted=4, CompetitionComplete=5, NoticeboardChanged=6, DiagnosticProbe=7, StartRequest=8, RequestResult=9, RaceIntegrityFault=10, SessionOperationRequest=11, SessionOperation=12, InputSettlement=13, PauseSettled=14, Paused=15, RestartScheduled=16, Resumed=17, FinishSettlement=18, FinishSettled=19, FalseStart=20, HistoryStored=21, StorageFault=22, EnduranceExpired=23, SetupRequest=24, Count=25 };
enum class ClientContext : uint8_t { Spectator, RaceDirectorSmug };
enum class RequestResult : uint8_t { Accepted, Rejected };
enum class RequestRejection : uint8_t { None, PermissionDenied, LifecycleNotStartable, InvalidRaceSetup, RequiredCapabilityUnavailable, SessionDefinitionUnavailable, LifecycleNotPausable, LifecycleNotRestartable, PauseSettlementPending, LifecycleNotRaceAgain, ConfirmationRequired, LifecycleNotAbandonable, StorageUnavailable, LifecycleNotResumable, LifecycleNotEndable, ProposalRevisionConflict };
enum class RaceIntegrityReason : uint8_t { None, InputEventDeliveryOverrun };
// These are request values, not Message Type values.  They are deliberately
// appended so the established Stage 11/12 operation values retain meaning.
enum class SessionOperation : uint8_t { Pause, HonourRestart, GridRestart, RaceAgain, RestartRace, EndRace, ClearHistory, ClearLane1Records, ClearLane2Records, ClearTrackRecord, ClearAllRecords, Resume, EndSession, SkipFinishDisplay };
enum class SetupChange : uint8_t { Replace = 1 };
enum class RestartMethod : uint8_t { None, Honour, Grid };
struct InputIdentity {
  uint32_t device;
  uint16_t capability;
  constexpr InputIdentity(uint32_t deviceValue=0, uint16_t capabilityValue=0):device(deviceValue),capability(capabilityValue) {} constexpr bool operator==(const InputIdentity& other) const { return device==other.device && capability==other.capability; }
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
  uint32_t raceEntryId = 0;
  Time lapTime = 0;
  InputIdentity input{};
  Configuration configuration{};
  LoadStatus loadStatus = LoadStatus::DefaultsMissing;
  uint32_t probe = 0;
  ClientContext clientContext = ClientContext::Spectator;
  RequestResult requestResult = RequestResult::Rejected;
  RequestRejection rejection = RequestRejection::None;
  RaceIntegrityReason integrityReason = RaceIntegrityReason::None;
  SessionOperation operation = SessionOperation::Pause;
  SessionMode sessionMode = SessionMode::LapRace;
  RestartMethod restartMethod = RestartMethod::None;
  union { uint32_t historySequence = 0; uint32_t proposalRevision; };
  union { uint32_t lapNumber = 0; uint32_t setupLapTarget; };
  union { uint16_t durationMinutes = 0; uint16_t setupDurationMinutes; };
  union { uint8_t finishPolicy = 0; uint8_t setupFinishPolicy; };
};
enum class Delivery { Delivered, Forbidden, Invalid, NoSubscribers, Full };
class Bus {
public:
  static constexpr size_t Participants = 12;
#if defined(PP_STAGE14A_ACCEPTANCE) || defined(PP_STAGE14B_ACCEPTANCE)
  // The deterministic four-entry acceptance image has a reduced queue depth
  // solely to keep its diagnostic fixture within the ESP32 DRAM budget.  The
  // production/default bus contract remains Depth=16.
  static constexpr size_t Depth = 4;
#else
  static constexpr size_t Depth = 16;
#endif
  struct Endpoint { uint16_t id; Endpoint(uint16_t value=0):id(value) {} };
  Endpoint attach(Role role) {
    if (count_ == Participants) return {};
    slots_[count_].role = role;
    return {uint16_t(++count_)};
  }
  bool subscribe(Endpoint e, Type t) {
    auto* s = slot(e);
    if (!s || unsigned(t) >= unsigned(Type::Count) || !(consumers(t) & mask(s->role))) return false;
    s->subscriptions |= uint32_t(1u << unsigned(t)); return true;
  }
  bool unsubscribe(Endpoint e, Type t) {
    auto* s = slot(e);
    if (!s || unsigned(t) >= unsigned(Type::Count) || !(consumers(t) & mask(s->role))) return false;
    s->subscriptions &= uint32_t(~(1u << unsigned(t))); return true;
  }
  Delivery publish(Endpoint e, Message m) {
    auto* sender = slot(e);
    if (!sender || unsigned(m.type) >= unsigned(Type::Count)) return Delivery::Invalid;
    if (!(publishers(m.type) & mask(sender->role))) return Delivery::Forbidden;
    if ((m.type == Type::LoadConfiguration || m.type == Type::ConfigurationLoaded || m.type == Type::StartRequest || m.type == Type::RequestResult || m.type == Type::SessionOperationRequest) && m.correlation == 0) return Delivery::Invalid;
    size_t recipients = 0;
    for (size_t i=0;i<count_;++i) if (interested(slots_[i], m.type)) {
      ++recipients;
      // These current presentation deliveries are individually best-effort:
      // Noticeboard change may be superseded, and presentation Facts can be
      // lost without changing authoritative operation. Future Presentation
      // message types remain reliable unless their contract says otherwise.
      if (slots_[i].size == Depth && !((slots_[i].role == Role::Presentation && presentationBestEffort(m.type)) || (slots_[i].role == Role::Diagnostics && m.type == Type::InputEvent) || (m.type == Type::RaceIntegrityFault && slots_[i].role == Role::RaceEngine))) return Delivery::Full;
    }
    if (!recipients) return Delivery::NoSubscribers;
    m.source = e.id;
    // Atomic fan-out: explicit failure, never a partially delivered publication.
    for (size_t i=0;i<count_;++i) if (interested(slots_[i], m.type)) {
      auto& s=slots_[i]; if(s.size==Depth&&((s.role==Role::Presentation&&presentationBestEffort(m.type))||(s.role==Role::Diagnostics&&m.type==Type::InputEvent)))continue;
      if(s.size==Depth&&m.type==Type::RaceIntegrityFault&&s.role==Role::RaceEngine){s.head=0;s.size=0;}
      s.queue[(s.head+s.size)%Depth]=m; ++s.size;
    }
    return Delivery::Delivered;
  }
  bool receive(Endpoint e, Message& out) {
    auto* s=slot(e); if (!s || !s->size) return false;
    out=s->queue[s->head]; s->head=(s->head+1)%Depth; --s->size; return true;
  }
  size_t queued(Endpoint e) const { const auto* s=slot(e); return s?s->size:0; }
private:
  struct Slot { Role role=Role::Diagnostics; uint32_t subscriptions=0; Message queue[Depth]{}; size_t head=0,size=0; };
  Slot slots_[Participants]{}; size_t count_=0;
  Slot* slot(Endpoint e) { return e.id && e.id<=count_ ? &slots_[e.id-1] : nullptr; }
  const Slot* slot(Endpoint e) const { return e.id && e.id<=count_ ? &slots_[e.id-1] : nullptr; }
  static bool interested(const Slot& s, Type t) { return s.subscriptions & (1u << unsigned(t)); }
  static bool presentationBestEffort(Type t) {
    switch(t) {
      case Type::GoScheduled:
      case Type::LapCompleted:
      case Type::CompetitionComplete:
      case Type::NoticeboardChanged: return true;
      default: return false;
    }
  }
  static uint16_t publishers(Type t) {
    switch(t) {
      case Type::LoadConfiguration: return mask(Role::Lifecycle);
      case Type::ConfigurationLoaded: return mask(Role::Memory);
      case Type::InputEvent: return mask(Role::Input);
      case Type::StartRequest: return mask(Role::Presentation);
      case Type::RequestResult: return mask(Role::RaceControl);
      case Type::RaceIntegrityFault: return mask(Role::Input);
      case Type::SessionOperationRequest: return mask(Role::Presentation);
      case Type::SessionOperation: return mask(Role::RaceControl);
      case Type::InputSettlement: return mask(Role::Input);
      case Type::PauseSettled: return mask(Role::RaceEngine);
      case Type::FinishSettlement: return mask(Role::RaceEngine);
      case Type::FinishSettled: return mask(Role::Input);
      case Type::Paused: case Type::RestartScheduled: case Type::Resumed: return mask(Role::RaceControl);
      case Type::GoScheduled: return mask(Role::RaceControl);
      case Type::LapCompleted: return mask(Role::RaceEngine);
      case Type::CompetitionComplete: return mask(Role::RaceEngine);
      case Type::NoticeboardChanged: return mask(Role::RaceControl)|mask(Role::RaceEngine);
      case Type::DiagnosticProbe: return mask(Role::Diagnostics);
      case Type::FalseStart: return mask(Role::RaceEngine);
      case Type::HistoryStored: case Type::StorageFault: return mask(Role::RaceEngine);
      case Type::EnduranceExpired: return mask(Role::RaceControl);
      case Type::SetupRequest: return mask(Role::Presentation);
      default: return 0;
    }
  }
  static uint16_t consumers(Type t) {
    switch(t) {
      case Type::LoadConfiguration: return mask(Role::Memory);
      case Type::ConfigurationLoaded: return mask(Role::Lifecycle);
      case Type::InputEvent: return mask(Role::RaceEngine)|mask(Role::Diagnostics);
      case Type::StartRequest: return mask(Role::RaceControl)|mask(Role::Diagnostics);
      case Type::RequestResult: return mask(Role::Presentation)|mask(Role::Diagnostics);
      case Type::RaceIntegrityFault: return mask(Role::RaceControl)|mask(Role::RaceEngine)|mask(Role::Diagnostics);
      case Type::SessionOperationRequest: return mask(Role::RaceControl)|mask(Role::Diagnostics);
      case Type::SetupRequest: return mask(Role::RaceControl)|mask(Role::Diagnostics);
      case Type::SessionOperation: return mask(Role::RaceEngine)|mask(Role::Input)|mask(Role::Diagnostics);
      case Type::InputSettlement: return mask(Role::RaceEngine)|mask(Role::Diagnostics);
      case Type::FinishSettlement: return mask(Role::Input)|mask(Role::Diagnostics);
      case Type::FinishSettled: return mask(Role::RaceEngine)|mask(Role::Diagnostics);
      case Type::PauseSettled: return mask(Role::RaceControl)|mask(Role::Diagnostics);
      case Type::Paused: case Type::RestartScheduled: case Type::Resumed: return mask(Role::Presentation)|mask(Role::Diagnostics);
      case Type::GoScheduled: return mask(Role::RaceEngine)|mask(Role::Presentation)|mask(Role::Diagnostics);
      case Type::LapCompleted: return mask(Role::Presentation)|mask(Role::Diagnostics);
      case Type::CompetitionComplete: return mask(Role::RaceControl)|mask(Role::Presentation)|mask(Role::Diagnostics);
      case Type::NoticeboardChanged: return mask(Role::Presentation)|mask(Role::Diagnostics);
      case Type::DiagnosticProbe: return mask(Role::Diagnostics);
      case Type::FalseStart: return mask(Role::RaceControl)|mask(Role::Presentation)|mask(Role::Diagnostics);
      case Type::HistoryStored: case Type::StorageFault: return mask(Role::RaceControl)|mask(Role::Presentation)|mask(Role::Diagnostics);
      case Type::EnduranceExpired: return mask(Role::RaceEngine)|mask(Role::RaceControl)|mask(Role::Presentation)|mask(Role::Diagnostics);
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
  SimulatedDetector()=default;
  explicit SimulatedDetector(InputIdentity identity):identity_(identity) {}
  void setIdentity(InputIdentity identity){identity_=identity;sourceActive_=false;armed_=true;stableSince_=0;}
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
  static constexpr size_t ProtectedDepth=8;
  static constexpr size_t MaxDetectors=PP_MAX_ENTRIES;
  static constexpr InputIdentity simulatedDetectorIdentity(uint16_t index=0){return InputIdentity(0x50500001u,uint16_t(index+1));}
  static constexpr InputIdentity simulatedDetectorBIdentity(){return simulatedDetectorIdentity(1);}
  InputModule(Bus& bus,Bus::Endpoint endpoint):endpoint(endpoint),bus_(bus){for(uint8_t i=0;i<MaxDetectors;++i)detectors_[i].setIdentity(simulatedDetectorIdentity(i));}
  Bus::Endpoint endpoint;
  void setSimulatedSource(bool active){setSimulatedSource(0,active);} void setSimulatedSourceB(bool active){setSimulatedSource(1,active);}
  void setSimulatedSource(uint8_t which,bool active){if(which<MaxDetectors)sources_[which]=active;}
  void tick(Time now){drainOperations();for(uint8_t i=0;i<MaxDetectors;++i)sampleSource(i,sources_[i],now);flush();settleIfReady();finishSettleIfReady();}
  void sampleSource(bool active,Time at){sampleSource(0,active,at);}
  void sampleSourceB(bool active,Time at){sampleSource(1,active,at);}
  void sampleSource(uint8_t which,bool active,Time at){if(which>=MaxDetectors)return;Time trigger=0;if(!detectors_[which].sample(active,at,trigger))return;++cleanTriggers_;Message e{};e.type=Type::InputEvent;e.input=detectors_[which].identity();e.relevantTime=trigger;e.eventId=++nextEventId_;enqueue(e);flush();}
  void flush(){if(faulted_)return;while(size_){Delivery d=bus_.publish(endpoint,backlog_[head_]);lastDelivery_=d;if(d==Delivery::Delivered){head_=(head_+1)%ProtectedDepth;--size_;continue;}if(d==Delivery::Full)return;fault(Time{});return;}}
  void settleIfReady(){if(!settlementPending_||size_||settlementPublished_)return;Message s{};s.type=Type::InputSettlement;s.relevantTime=settlementAt_;Delivery d=bus_.publish(endpoint,s);lastDelivery_=d;if(d==Delivery::Delivered)settlementPublished_=true;else if(d!=Delivery::Full)fault(settlementAt_);}
  void sessionOperation(const Message&m){if(m.operation==SessionOperation::Pause){settlementPending_=true;settlementAt_=m.relevantTime;settlementPublished_=false;settleIfReady();}else if(m.operation==SessionOperation::HonourRestart||m.operation==SessionOperation::GridRestart){settlementPending_=false;settlementPublished_=false;settlementAt_=0;}}
  void drainOperations(){Message m;while(bus_.receive(endpoint,m)){if(m.type==Type::SessionOperation)sessionOperation(m);else if(m.type==Type::FinishSettlement){if(!finishSettlementPending_||m.relevantTime!=finishSettlementAt_)finishSettlementPublished_=false;finishSettlementPending_=true;finishSettlementAt_=m.relevantTime;}}} void finishSettleIfReady(){if(!finishSettlementPending_||size_||finishSettlementPublished_||faulted_)return;Message s{};s.type=Type::FinishSettled;s.relevantTime=finishSettlementAt_;if(bus_.publish(endpoint,s)==Delivery::Delivered){finishSettlementPublished_=true;finishSettlementPending_=false;}}
  uint32_t cleanTriggers()const{return cleanTriggers_;}Delivery lastDelivery()const{return lastDelivery_;}size_t protectedBacklogDepth()const{return size_;}size_t maximumProtectedBacklogDepth()const{return maxDepth_;}bool faulted()const{return faulted_;}
private:
  void enqueue(const Message&e){if(faulted_)return;if(size_==ProtectedDepth){fault(e.relevantTime);return;}backlog_[(head_+size_)%ProtectedDepth]=e;++size_;if(size_>maxDepth_)maxDepth_=size_;}
  void fault(Time at){if(faulted_)return;faulted_=true;Message f{};f.type=Type::RaceIntegrityFault;f.relevantTime=at;f.integrityReason=RaceIntegrityReason::InputEventDeliveryOverrun;bus_.publish(endpoint,f);}
  Bus& bus_;SimulatedDetector detectors_[MaxDetectors]{};bool sources_[MaxDetectors]{};Message backlog_[ProtectedDepth]{};size_t head_=0,size_=0,maxDepth_=0;uint32_t cleanTriggers_=0,nextEventId_=0;Delivery lastDelivery_=Delivery::NoSubscribers;bool faulted_=false;bool settlementPending_=false,settlementPublished_=false;Time settlementAt_=0;bool finishSettlementPending_=false,finishSettlementPublished_=false;Time finishSettlementAt_=0;
};
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
