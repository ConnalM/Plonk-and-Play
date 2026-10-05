#pragma once
#include <WiFi.h>
#include <esp_http_server.h>
#include <esp_system.h>
#include <stdlib.h>
#include <stdarg.h>
#include "noticeboard.h"
#include "system_time.h"
namespace pp {
// Browser identity and the retained Master binding are Browser Interface State.
// Race Control receives only the logical ClientContext attached to a Bus message.
class AuthorityStore {
public:
  virtual bool loadMaster(uint64_t& fingerprint) = 0;
  virtual bool saveMaster(uint64_t fingerprint) = 0;
  virtual ~AuthorityStore() = default;
};
class VolatileAuthorityStore final : public AuthorityStore {
public:
  bool loadMaster(uint64_t& fingerprint) override {
    if (!set_) return false;
    fingerprint = fingerprint_;
    return true;
  }
  bool saveMaster(uint64_t fingerprint) override {
    if (!fingerprint) return false;
    fingerprint_ = fingerprint;
    set_ = true;
    return true;
  }
private:
  uint64_t fingerprint_ = 0;
  bool set_ = false;
};
class BrowserInterface {
public:
  struct ResultView {
    bool ready = false;
    RequestResult result = RequestResult::Rejected;
    RequestRejection rejection = RequestRejection::None;
  };
  BrowserInterface(Bus& bus, Bus::Endpoint endpoint, const Noticeboard& noticeboard,
                   AuthorityStore* authority = nullptr, HistoryStore* history = nullptr,
                   TrackRecordStore* records = nullptr)
      : bus_(bus), endpoint_(endpoint), noticeboard_(noticeboard),
        authority_(authority ? authority : &fallback_), history_(history), records_(records) {
    uint64_t persisted = 0;
    if (authority_->loadMaster(persisted) && persisted) masterFingerprint_ = persisted;
  }
  void begin() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin("Wokwi-GUEST", "", 6);
    started_ = true;
  }
  void tick() {
    Message message;
    while (bus_.receive(endpoint_, message)) {
      if (message.type == Type::NoticeboardChanged) ++noticeRevision_;
      else if (message.type == Type::LapCompleted) {
        lastFact_ = message;
        hasFact_ = true;
        ++factRevision_;
      } else if (message.type == Type::GoScheduled) {
        scheduledGo_ = message.relevantTime;
      } else if (message.type == Type::Paused || message.type == Type::RestartScheduled || message.type == Type::Resumed || message.type == Type::FalseStart || message.type == Type::HistoryStored || message.type == Type::StorageFault) {
        lastFact_ = message; hasFact_ = true; ++factRevision_;
      } else if (message.type == Type::RequestResult) {
        recordResult(message);
      }
    }
    if (started_ && !server_ && WiFi.status() == WL_CONNECTED) startServer();
  }
  // The public overload is retained for non-Browser test fixtures. HTTP calls
  // always include their server-resolved owner token below.
  bool submitStart(uint32_t visibleCorrelation, ClientContext context) {
    return submitStart(visibleCorrelation, context, 0);
  }
  bool submitStart(uint32_t visibleCorrelation, ClientContext context, uint64_t owner, SessionMode mode=SessionMode::LapRace) {
    if (!visibleCorrelation) return false;
    Message request{};
    request.type = Type::StartRequest;
    request.correlation = nextInternalCorrelation();
    request.clientContext = context;
    request.sessionMode = mode;
    if (bus_.publish(endpoint_, request) != Delivery::Delivered) return false;
    Pending& pending = pending_[nextPending_++ % ResultCapacity];
    pending.internal = request.correlation;
    pending.visible = visibleCorrelation;
    pending.owner = owner;
    pending.ready = true;
    return true;
  }
  void clearForFixture(){masterFingerprint_=0;clearRaceForFixture();}
  void clearRaceForFixture(){for(auto& p:pending_)p.ready=false;for(auto& r:results_)r.ready=false;hasFact_=false;lastFact_={};noticeRevision_=0;factRevision_=0;scheduledGo_=0;}
  void setFixtureCallbacks(bool (*pass)(uint8_t), void (*reset)(), bool (*setup)(uint32_t)=nullptr){fixturePass_=pass;fixtureReset_=reset;fixtureSetup_=setup;}
  bool submitOperation(uint32_t visibleCorrelation, SessionOperation operation, ClientContext context, uint64_t owner, bool confirmed=false) {
    if (!visibleCorrelation) return false;
    Message request{}; request.type=Type::SessionOperationRequest; request.correlation=nextInternalCorrelation(); request.operation=operation; request.clientContext=context;request.probe=confirmed?1:0;
    if (bus_.publish(endpoint_,request)!=Delivery::Delivered) return false;
    Pending& pending=pending_[nextPending_++%ResultCapacity]; pending.internal=request.correlation; pending.visible=visibleCorrelation; pending.owner=owner; pending.ready=true; return true;
  }
  bool requestResult(uint32_t visibleCorrelation, ResultView& result) const {
    return requestResult(visibleCorrelation, 0, result);
  }
  bool requestResult(uint32_t visibleCorrelation, uint64_t owner, ResultView& result) const {
    for (const StoredResult& candidate : results_) {
      if (candidate.ready && candidate.visible == visibleCorrelation && candidate.owner == owner) {
        result.ready = true;
        result.result = candidate.result;
        result.rejection = candidate.rejection;
        return true;
      }
    }
    return false;
  }
  // Setting the in-RAM binding before persistence prevents another request in
  // this Browser Interface instance from racing in and replacing it. A failed
  // persistence attempt restores the no-Master state.
  bool bootstrap(uint64_t token) {
    if (!token || masterFingerprint_) return false;
    const uint64_t candidate = fingerprint(token);
    masterFingerprint_ = candidate;
    if (authority_->saveMaster(candidate)) return true;
    masterFingerprint_ = 0;
    return false;
  }
  bool hasMaster() const { return masterFingerprint_ != 0; }
  ClientContext context(uint64_t token) const {
    return token && masterFingerprint_ == fingerprint(token)
               ? ClientContext::RaceDirectorSmug
               : ClientContext::Spectator;
  }
  NoticeboardState current() const { return noticeboard_.current(); }
  uint32_t noticeRevision() const { return noticeRevision_; }
  uint32_t factRevision() const { return factRevision_; }
  bool lastFact(Message& fact) const {
    if (!hasFact_) return false;
    fact = lastFact_;
    return true;
  }
  Time scheduledGo() const { return scheduledGo_; }
  bool serverReady() const { return server_ != nullptr; }
  int serverStartError() const { return serverStartError_; }
  // These bounded serializers are the same production formatting boundary
  // used by the HTTP routes. They are public only so deterministic acceptance
  // fixtures can validate complete payloads without fabricating HTTP state.
  static constexpr size_t StateJsonCapacity = 4096;
  static constexpr size_t ResultsJsonCapacity = 4096;
  static constexpr size_t RecordsJsonCapacity = 2048;
  bool serializeStateForAcceptance(char* out,size_t cap,size_t&length)const{return serializeState(current(),systemTime(),out,cap,length);}
  bool serializeResultsForAcceptance(char* out,size_t cap,size_t&length)const{RaceEngineModule::CompletedRaceResult loaded{};return serializeResults(displayResult(loaded),out,cap,length);}
  bool serializeDetailsForAcceptance(char* out,size_t cap,size_t&length)const{RaceEngineModule::CompletedRaceResult loaded{};return serializeDetails(displayResult(loaded),out,cap,length);}
  bool serializeRecordsForAcceptance(char* out,size_t cap,size_t&length)const{return serializeRecords(records_,current().entryCount,out,cap,length);}
  static bool serializeStateForAcceptance(const NoticeboardState& value,Time now,char*out,size_t cap,size_t&length){return serializeState(value,now,out,cap,length);}
  static bool serializeResultsForAcceptance(const RaceEngineModule::CompletedRaceResult& value,char*out,size_t cap,size_t&length){return serializeResults(value,out,cap,length);}
  static bool serializeDetailsForAcceptance(const RaceEngineModule::CompletedRaceResult& value,char*out,size_t cap,size_t&length){return serializeDetails(value,out,cap,length);}
  static bool serializeRecordsForAcceptance(TrackRecordStore* records,uint8_t count,char*out,size_t cap,size_t&length){return serializeRecords(records,count,out,cap,length);}
private:
  static constexpr size_t ResultCapacity = 8;
  struct StoredResult {
    uint32_t internal = 0;
    uint32_t visible = 0;
    uint64_t owner = 0;
    RequestResult result = RequestResult::Rejected;
    RequestRejection rejection = RequestRejection::None;
    bool ready = false;
  };
  struct Pending {
    uint32_t internal = 0;
    uint32_t visible = 0;
    uint64_t owner = 0;
    bool ready = false;
  };
  struct DestructiveConfirmation { uint64_t owner=0; SessionOperation operation=SessionOperation::RestartRace; bool open=false; };
  static BrowserInterface*& instance() {
    static BrowserInterface* value = nullptr;
    return value;
  }
  Bus& bus_;
  Bus::Endpoint endpoint_;
  const Noticeboard& noticeboard_;
  AuthorityStore* authority_;
  HistoryStore* history_ = nullptr;
  TrackRecordStore* records_ = nullptr;
  VolatileAuthorityStore fallback_;
  httpd_handle_t server_ = nullptr;
  uint64_t masterFingerprint_ = 0;
  uint32_t noticeRevision_ = 0;
  uint32_t factRevision_ = 0;
  uint32_t nextCorrelation_ = 1;
  Message lastFact_{};
  bool hasFact_ = false;
  bool started_ = false;
  Time scheduledGo_ = 0;
  StoredResult results_[ResultCapacity]{};
  Pending pending_[ResultCapacity]{};
  size_t nextResult_ = 0;
  size_t nextPending_ = 0;
  int serverStartError_ = 0;
  DestructiveConfirmation confirmation_{};
  bool (*fixturePass_)(uint8_t) = nullptr;
  void (*fixtureReset_)() = nullptr;
  bool (*fixtureSetup_)(uint32_t) = nullptr;
  const RaceEngineModule::CompletedRaceResult& displayResult(RaceEngineModule::CompletedRaceResult& loaded) const {
    const auto& current=noticeboard_.completedResult();
    if(current.sealed || !history_) return current;
    size_t bytes=0;uint32_t sequence=0;
    if(history_->loadNewest(0,reinterpret_cast<uint8_t*>(&loaded),sizeof(loaded),bytes,sequence) && bytes==sizeof(loaded) && loaded.sealed && loaded.formatVersion==RaceEngineModule::ResultFormatVersion)return loaded;
    return current;
  }
  static uint64_t fingerprint(uint64_t token) {
    token ^= token >> 33;
    token *= 0xff51afd7ed558ccdULL;
    token ^= token >> 33;
    return token ? token : 1;
  }
  uint32_t nextInternalCorrelation() {
    const uint32_t value = nextCorrelation_++;
    return value ? value : nextCorrelation_++;
  }
  static const char* lifecycle(SessionLifecycle value) {
    switch (value) {
      case SessionLifecycle::Ready: return "READY";
      case SessionLifecycle::Starting: return "STARTING";
      case SessionLifecycle::Racing: return "RACING";
      case SessionLifecycle::Paused: return "PAUSED";
      case SessionLifecycle::Restarting: return "RESTARTING";
      case SessionLifecycle::Finished: return "FINISHED";
      case SessionLifecycle::Faulted: return "FAULTED";
    }
    return "UNKNOWN";
  }
  static const char* rejectionText(RequestRejection value) {
    switch (value) {
      case RequestRejection::PermissionDenied: return "START permission denied";
      case RequestRejection::LifecycleNotStartable: return "START is not valid in the current lifecycle state";
      case RequestRejection::InvalidRaceSetup: return "invalid session setup";
      case RequestRejection::RequiredCapabilityUnavailable: return "required Start/Finish capability unavailable";
      case RequestRejection::SessionDefinitionUnavailable: return "unable to establish Session Definition";
      case RequestRejection::LifecycleNotPausable: return "PAUSE is not valid in the current lifecycle state";
      case RequestRejection::LifecycleNotRestartable: return "restart is not valid in the current lifecycle state";
      case RequestRejection::PauseSettlementPending: return "PAUSE settlement is still pending";
      case RequestRejection::LifecycleNotRaceAgain: return "RACE AGAIN is only available after a finished race";
      case RequestRejection::ConfirmationRequired: return "confirmation required";
      case RequestRejection::LifecycleNotAbandonable: return "operation is only available after PAUSE settlement";
      case RequestRejection::StorageUnavailable: return "persistent storage is unavailable";
      case RequestRejection::LifecycleNotResumable: return "RESUME is not valid in the current lifecycle state";
      case RequestRejection::LifecycleNotEndable: return "END SESSION is not valid in the current lifecycle state";
      default: return "";
    }
  }
  void recordResult(const Message& message) {
    for (const Pending& pending : pending_) {
      if (!pending.ready || pending.internal != message.correlation) continue;
      StoredResult& stored = results_[nextResult_++ % ResultCapacity];
      stored.internal = message.correlation;
      stored.visible = pending.visible;
      stored.owner = pending.owner;
      stored.result = message.requestResult;
      stored.rejection = message.rejection;
      stored.ready = true;
      return;
    }
  }
  static uint64_t token(httpd_req_t* request, bool& fresh) {
    char cookie[128]{};
    if (httpd_req_get_hdr_value_str(request, "Cookie", cookie, sizeof(cookie)) == ESP_OK) {
      const char* key = strstr(cookie, "pp_browser=");
      if (key) {
        char* end = nullptr;
        const uint64_t value = strtoull(key + 11, &end, 16);
        if (end && end != key + 11 && value) return value;
      }
    }
    fresh = true;
    const uint64_t value = (uint64_t(esp_random()) << 32) | esp_random();
    return value ? value : 1;
  }
  static void setCookie(httpd_req_t* request, uint64_t tokenValue) {
    // ESP-IDF keeps the header value until it sends the response. A small ring
    // keeps values alive through the response without placing any
    // Browser identity in race-control State.
    static char headers[16][112];
    static uint32_t nextHeader = 0;
    char* header = headers[nextHeader++ % 16];
    const uint32_t hi = uint32_t(tokenValue >> 32);
    const uint32_t lo = uint32_t(tokenValue);
    snprintf(header, 112,
             "pp_browser=%08lx%08lx; Max-Age=31536000; Path=/; HttpOnly; SameSite=Lax",
             (unsigned long)hi, (unsigned long)lo);
    httpd_resp_set_hdr(request, "Set-Cookie", header);
  }
  static uint64_t client(httpd_req_t* request) {
    bool fresh = false;
    const uint64_t value = token(request, fresh);
    if (fresh) setCookie(request, value);
    return value;
  }
  static bool appendJson(char*out,size_t cap,size_t&n,const char*format,...){
    if(!out||n>=cap)return false;va_list args;va_start(args,format);const int written=vsnprintf(out+n,cap-n,format,args);va_end(args);
    if(written<0||size_t(written)>=cap-n){n=cap;return false;}n+=size_t(written);return true;
  }
  static bool serializeState(const NoticeboardState& value,Time now,char*out,size_t cap,size_t&length){
    size_t n=0;bool ok=appendJson(out,cap,n,"{\"systemTime\":%llu,\"lifecycle\":\"%s\",\"sessionMode\":\"%s\",\"entryCount\":%u,\"raceEntryId\":%lu,\"laps\":%lu,\"lapTarget\":%lu,\"hasLap\":%s,\"lastLapTime\":%llu,\"sessionFastestLap\":%llu,\"scheduledGo\":%llu,\"redLightCount\":%u,\"redIntervalUs\":%llu,\"finalDelayUs\":%llu,\"startSignal\":%u,\"pauseEffectiveAt\":%llu,\"scheduledRestartAt\":%llu,\"restartMethod\":\"%s\",\"resultValid\":%s,\"raceIntegrity\":\"%s\",\"resultSealed\":%s,\"winningTime\":%llu,\"finishTime\":%llu,\"fastestLap\":%llu,\"historySequence\":%lu,\"persistencePending\":%s,\"persistenceFault\":%s,\"entries\":[",
      (unsigned long long)now,lifecycle(value.lifecycle),value.sessionMode==SessionMode::OpenPractice?"OPEN_PRACTICE":"LAP_RACE",unsigned(value.entryCount),(unsigned long)value.raceEntryId,(unsigned long)value.laps,(unsigned long)value.lapTarget,value.hasLap?"true":"false",(unsigned long long)value.lastLapTime,(unsigned long long)value.sessionFastestLap,(unsigned long long)value.scheduledGo,unsigned(value.redLightCount),(unsigned long long)value.redIntervalUs,(unsigned long long)value.finalDelayUs,unsigned(value.startSignal),(unsigned long long)value.pauseEffectiveAt,(unsigned long long)value.scheduledRestartAt,value.restartMethod==RestartMethod::Honour?"HONOUR":value.restartMethod==RestartMethod::Grid?"GRID":"NONE",value.resultValid?"true":"false",value.raceIntegrityFaulted?"FAULTED":"OK",value.resultSealed?"true":"false",(unsigned long long)value.winningTime,(unsigned long long)value.finishTime,(unsigned long long)value.fastestLap,(unsigned long)value.historySequence,value.persistencePending?"true":"false",value.persistenceFault?"true":"false");
    if(value.entryCount>PP_MAX_ENTRIES)ok=false;const uint8_t count=value.entryCount>PP_MAX_ENTRIES?PP_MAX_ENTRIES:value.entryCount;
    for(uint8_t i=0;i<count;++i){const auto&e=value.entries[i];ok=appendJson(out,cap,n,"%s{\"raceEntryId\":%lu,\"lane\":%u,\"laps\":%lu,\"lastLapTime\":%llu,\"bestLapTime\":%llu,\"hasLap\":%s,\"waitingForTimingOrigin\":%s}",i?",":"",(unsigned long)e.raceEntryId,unsigned(i+1),(unsigned long)e.laps,(unsigned long long)e.lastLapTime,(unsigned long long)e.bestLapTime,e.hasLap?"true":"false",e.waitingForTimingOrigin?"true":"false")&&ok;}
    ok=appendJson(out,cap,n,"]}")&&ok;length=n;return ok;
  }
  static bool serializeResults(const RaceEngineModule::CompletedRaceResult&r,char*out,size_t cap,size_t&length){
    size_t n=0;bool ok=appendJson(out,cap,n,"{\"sealed\":%s,\"valid\":%s,\"lapTarget\":%lu,\"winningTime\":%llu,\"finishTime\":%llu,\"finishBehaviour\":\"%s\",\"deadHeat\":%s,\"fastestLap\":%llu,\"fastestEntryId\":%lu,\"fastestLapTied\":%s,\"entries\":[",r.sealed?"true":"false",r.valid?"true":"false",(unsigned long)r.lapTarget,(unsigned long long)r.winningTime,(unsigned long long)r.finishTime,finishBehaviour(r.behaviour),r.deadHeat?"true":"false",(unsigned long long)r.fastestLap,(unsigned long)r.fastestEntryId,r.fastestLapTied?"true":"false");
    if(r.entryCount>PP_MAX_ENTRIES)ok=false;const uint8_t count=r.entryCount>PP_MAX_ENTRIES?PP_MAX_ENTRIES:r.entryCount;
    for(uint8_t i=0;i<count;++i){const auto&e=r.entries[i];ok=appendJson(out,cap,n,"%s{\"raceEntryId\":%lu,\"lane\":%u,\"laps\":%lu,\"rank\":%lu,\"tied\":%s,\"lapsBehind\":%lu,\"completed\":%s,\"completionTime\":%llu,\"bestLap\":%llu}",i?",":"",(unsigned long)e.raceEntryId,unsigned(e.lane),(unsigned long)e.laps,(unsigned long)e.rank,e.tied?"true":"false",(unsigned long)e.lapsBehind,e.completed?"true":"false",(unsigned long long)e.completionTime,(unsigned long long)e.bestLap)&&ok;}
    ok=appendJson(out,cap,n,"]}")&&ok;length=n;return ok;
  }
  static bool serializeDetails(const RaceEngineModule::CompletedRaceResult&r,char*out,size_t cap,size_t&length){
    size_t n=0;bool ok=appendJson(out,cap,n,"{\"sealed\":%s,\"lapTarget\":%lu,\"entries\":[",r.sealed?"true":"false",(unsigned long)r.lapTarget);
    if(r.entryCount>PP_MAX_ENTRIES)ok=false;const uint8_t count=r.entryCount>PP_MAX_ENTRIES?PP_MAX_ENTRIES:r.entryCount;
    for(uint8_t i=0;i<count;++i){const auto&e=r.entries[i];ok=appendJson(out,cap,n,"%s{\"raceEntryId\":%lu,\"lane\":%u,\"laps\":%lu,\"rank\":%lu,\"records\":[",i?",":"",(unsigned long)e.raceEntryId,unsigned(e.lane),(unsigned long)e.laps,(unsigned long)e.rank)&&ok;for(uint8_t j=0;j<e.recordCount&&j<RaceEngineModule::MaxLaps;++j){const auto&lap=e.records[j];ok=appendJson(out,cap,n,"%s{\"lapNumber\":%lu,\"startTime\":%llu,\"finishTime\":%llu,\"lapTime\":%llu,\"valid\":%s}",j?",":"",(unsigned long)lap.lapNumber,(unsigned long long)lap.startTime,(unsigned long long)lap.finishTime,(unsigned long long)lap.lapTime,lap.valid?"true":"false")&&ok;}ok=appendJson(out,cap,n,"]}")&&ok;}
    ok=appendJson(out,cap,n,"]}")&&ok;length=n;return ok;
  }
  static bool serializeRecords(TrackRecordStore* records,uint8_t count,char*out,size_t cap,size_t&length){
    size_t n=0;const uint32_t era=records?records->era():0;const Time lane1=records?records->lanePb(1):0;const Time lane2=records?records->lanePb(2):0;const Time track=records?records->trackRecord():0;bool ok=appendJson(out,cap,n,"{\"track\":\"DEFAULT TRACK\",\"era\":%lu,\"lane1Pb\":%llu,\"lane2Pb\":%llu,\"trackRecord\":%llu,\"entries\":[",(unsigned long)era,(unsigned long long)lane1,(unsigned long long)lane2,(unsigned long long)track);if(count>PP_MAX_ENTRIES)ok=false;const uint8_t bounded=count>PP_MAX_ENTRIES?PP_MAX_ENTRIES:count;for(uint8_t i=1;i<=bounded;++i){const Time pb=records?records->lanePb(i):0;ok=appendJson(out,cap,n,"%s{\"lane\":%u,\"pb\":%llu}",i==1?"":",",unsigned(i),(unsigned long long)pb)&&ok;}ok=appendJson(out,cap,n,"]}")&&ok;length=n;return ok;
  }
  static esp_err_t state(httpd_req_t* request) {
    client(request);
    const NoticeboardState value = instance()->current();
    char* json=static_cast<char*>(malloc(StateJsonCapacity)); size_t length=0;
    if(!json){httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"state buffer unavailable");return ESP_FAIL;}
    if(!serializeState(value,systemTime(),json,StateJsonCapacity,length)){free(json);httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"state entry count exceeds capacity");return ESP_FAIL;}
    httpd_resp_set_type(request,"application/json"); httpd_resp_set_hdr(request,"Cache-Control","no-store");
    const esp_err_t sent=httpd_resp_sendstr(request,json);free(json);return sent;
  }
  static const char* finishBehaviour(LapFinishBehaviour value) {
    return value==LapFinishBehaviour::CompleteCurrentLap?"COMPLETE_CURRENT_LAP":value==LapFinishBehaviour::CompleteFullRaceDistance?"COMPLETE_FULL_RACE_DISTANCE":"IMMEDIATE";
  }
  static esp_err_t resultsRoute(httpd_req_t* request) {
    client(request);auto* loaded=static_cast<RaceEngineModule::CompletedRaceResult*>(malloc(sizeof(RaceEngineModule::CompletedRaceResult)));char* json=static_cast<char*>(malloc(ResultsJsonCapacity));
    if(!loaded||!json){free(loaded);free(json);httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"result buffer unavailable");return ESP_FAIL;}const auto&r=instance()->displayResult(*loaded);size_t length=0;
    if(!serializeResults(r,json,ResultsJsonCapacity,length)){free(loaded);free(json);httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"result entry count exceeds capacity");return ESP_FAIL;}
    httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");const esp_err_t sent=httpd_resp_sendstr(request,json);free(loaded);free(json);return sent;
  }
  static esp_err_t detailsRoute(httpd_req_t* request) {
     client(request);auto* loaded=static_cast<RaceEngineModule::CompletedRaceResult*>(malloc(sizeof(RaceEngineModule::CompletedRaceResult)));char* chunk=static_cast<char*>(malloc(320));if(!loaded||!chunk){free(loaded);free(chunk);httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"details buffer unavailable");return ESP_FAIL;}const auto&r=instance()->displayResult(*loaded);httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");snprintf(chunk,320,"{\"sealed\":%s,\"lapTarget\":%lu,\"entries\":[",r.sealed?"true":"false",(unsigned long)r.lapTarget);httpd_resp_send_chunk(request,chunk,HTTPD_RESP_USE_STRLEN);
    for(uint8_t i=0;i<r.entryCount;++i){const auto&e=r.entries[i];snprintf(chunk,320,"%s{\"raceEntryId\":%lu,\"lane\":%u,\"laps\":%lu,\"rank\":%lu,\"records\":[",i?",":"",(unsigned long)e.raceEntryId,unsigned(e.lane),(unsigned long)e.laps,(unsigned long)e.rank);httpd_resp_send_chunk(request,chunk,HTTPD_RESP_USE_STRLEN);for(uint8_t j=0;j<e.recordCount;++j){const auto&lap=e.records[j];snprintf(chunk,320,"%s{\"lapNumber\":%lu,\"startTime\":%llu,\"finishTime\":%llu,\"lapTime\":%llu,\"valid\":%s}",j?",":"",(unsigned long)lap.lapNumber,(unsigned long long)lap.startTime,(unsigned long long)lap.finishTime,(unsigned long long)lap.lapTime,lap.valid?"true":"false");httpd_resp_send_chunk(request,chunk,HTTPD_RESP_USE_STRLEN);}httpd_resp_send_chunk(request,"]}",2);}
    httpd_resp_send_chunk(request,"]}",2);const esp_err_t sent=httpd_resp_send_chunk(request,nullptr,0);free(loaded);free(chunk);return sent;
  }
  static esp_err_t historyRoute(httpd_req_t* request) {
     client(request); BrowserInterface* browser=instance();
    char* json=static_cast<char*>(malloc(768));auto* stored=static_cast<RaceEngineModule::CompletedRaceResult*>(malloc(sizeof(RaceEngineModule::CompletedRaceResult)));if(!json||!stored){free(json);free(stored);httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"history buffer unavailable");return ESP_FAIL;}int n=snprintf(json,768,"{\"entries\":[");
     if(browser->history_)for(uint8_t i=0;i<browser->history_->count()&&n<768;++i){size_t bytes=0;uint32_t sequence=0;if(browser->history_->loadNewest(i,reinterpret_cast<uint8_t*>(stored),sizeof(*stored),bytes,sequence)&&bytes==sizeof(*stored)&&stored->sealed)n+=snprintf(json+n,768-n,"%s{\"sequence\":%lu,\"sealed\":true,\"lapTarget\":%lu,\"winningTime\":%llu,\"finishTime\":%llu}",i?",":"",(unsigned long)sequence,(unsigned long)stored->lapTarget,(unsigned long long)stored->winningTime,(unsigned long long)stored->finishTime);}
    snprintf(json+n,768-n,"]}");
    httpd_resp_set_type(request,"application/json"); const esp_err_t sent=httpd_resp_sendstr(request,json);free(json);free(stored);return sent;
  }
  static esp_err_t recordsRoute(httpd_req_t* request) {
    client(request); BrowserInterface* browser=instance();
    char* json=static_cast<char*>(malloc(RecordsJsonCapacity));size_t length=0;
    if(!json){httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"records buffer unavailable");return ESP_FAIL;}
    if(!serializeRecords(browser->records_,browser->noticeboard_.current().entryCount,json,RecordsJsonCapacity,length)){free(json);httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"record entry count exceeds capacity");return ESP_FAIL;}
    httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");const esp_err_t sent=httpd_resp_sendstr(request,json);free(json);return sent;
  }
  static esp_err_t notice(httpd_req_t* request) {
    client(request);
    char json[96];
    snprintf(json, sizeof(json), "{\"type\":\"NOTICEBOARD_CHANGED\",\"revision\":%lu}",
             (unsigned long)instance()->noticeRevision());
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, json);
  }
  static esp_err_t fact(httpd_req_t* request) {
    client(request);
    Message fact{};
    char json[256];
    if (!instance()->lastFact(fact)) {
      snprintf(json, sizeof(json), "{\"type\":\"NONE\",\"revision\":%lu}",
               (unsigned long)instance()->factRevision());
    } else if (fact.type==Type::LapCompleted) {
      snprintf(json, sizeof(json),
        "{\"type\":\"LAP_COMPLETED\",\"revision\":%lu,\"raceEntryId\":%lu,\"lapNumber\":%lu,\"lapTime\":%llu,\"relevantTime\":%llu}",
        (unsigned long)instance()->factRevision(), (unsigned long)fact.raceEntryId,
        (unsigned long)fact.lapNumber, (unsigned long long)fact.lapTime,
        (unsigned long long)fact.relevantTime);
    } else if (fact.type==Type::FalseStart) {
      const char* policy=fact.probe==2?"+1 LAP":fact.probe==1?"WARNING":"OFF";
      snprintf(json,sizeof(json),"{\"type\":\"FALSE_START\",\"revision\":%lu,\"raceEntryId\":%lu,\"policy\":\"%s\",\"relevantTime\":%llu}",(unsigned long)instance()->factRevision(),(unsigned long)fact.raceEntryId,policy,(unsigned long long)fact.relevantTime);
    } else {
      const char* name=fact.type==Type::Paused?"PAUSED":fact.type==Type::RestartScheduled?"RESTART_SCHEDULED":fact.type==Type::Resumed?"RESUMED":fact.type==Type::FalseStart?"FALSE_START":fact.type==Type::HistoryStored?"HISTORY_STORED":"STORAGE_FAULT";
      snprintf(json,sizeof(json),"{\"type\":\"%s\",\"revision\":%lu,\"relevantTime\":%llu}",name,(unsigned long)instance()->factRevision(),(unsigned long long)fact.relevantTime);
    }
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, json);
  }
  static bool correlationFromBody(httpd_req_t* request, uint32_t& correlation, SessionMode* requestedMode=nullptr) {
    if (request->content_len <= 0 || request->content_len >= 256) return false;
    char body[256]{};
    int received = 0;
    while (received < request->content_len) {
      const int part = httpd_req_recv(request, body + received, request->content_len - received);
      if (part <= 0) return false;
      received += part;
    }
    const char* key = strstr(body, "\"correlationId\"");
    const char* colon = key ? strchr(key, ':') : nullptr;
    if (!colon) return false;
    char* end = nullptr;
    const unsigned long value = strtoul(colon + 1, &end, 10);
    if (!end || end == colon + 1 || value == 0 || value > UINT32_MAX) return false;
    correlation = uint32_t(value);
    if(requestedMode){const char* mode=strstr(body,"OPEN_PRACTICE");*requestedMode=mode?SessionMode::OpenPractice:SessionMode::LapRace;}
    return true;
  }
  static esp_err_t bootstrapRoute(httpd_req_t* request) {
    const uint64_t owner = client(request);
    if (request->content_len != 0) {
      httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "empty body required");
      return ESP_FAIL;
    }
    if (!instance()->bootstrap(owner)) {
      httpd_resp_set_status(request, "409 Conflict");
      return httpd_resp_sendstr(request, "{\"bootstrap\":false}");
    }
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, "{\"bootstrap\":true,\"role\":\"Race Director\"}");
  }
  static esp_err_t contextRoute(httpd_req_t* request) {
    const uint64_t owner = client(request);
    char json[96];
    snprintf(json, sizeof(json), "{\"role\":\"%s\",\"hasMaster\":%s}",
             instance()->context(owner) == ClientContext::RaceDirectorSmug ? "Race Director" : "Spectator",
             instance()->hasMaster() ? "true" : "false");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, json);
  }
  static esp_err_t startRoute(httpd_req_t* request) {
    uint32_t correlation = 0;
    SessionMode requestedMode=SessionMode::LapRace;
    if (!correlationFromBody(request, correlation, &requestedMode)) {
      httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "correlationId required");
      return ESP_FAIL;
    }
    const uint64_t owner = client(request);
    if (!instance()->submitStart(correlation, instance()->context(owner), owner, requestedMode)) {
      httpd_resp_set_status(request, "503 Service Unavailable");
      return httpd_resp_sendstr(request, "request unavailable");
    }
    char json[96];
    snprintf(json, sizeof(json), "{\"submitted\":true,\"correlationId\":%lu}",
             (unsigned long)correlation);
    httpd_resp_set_status(request, "202 Accepted");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, json);
  }
  static esp_err_t operationRoute(httpd_req_t* request, SessionOperation operation, bool confirmed=false) {
    uint32_t correlation=0;if(!correlationFromBody(request,correlation)){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"correlationId required");return ESP_FAIL;}
    const uint64_t owner=client(request);
    if(!instance()->submitOperation(correlation,operation,instance()->context(owner),owner,confirmed)){httpd_resp_set_status(request,"503 Service Unavailable");return httpd_resp_sendstr(request,"request unavailable");}
    char json[96];snprintf(json,sizeof(json),"{\"submitted\":true,\"correlationId\":%lu}",(unsigned long)correlation);httpd_resp_set_status(request,"202 Accepted");httpd_resp_set_type(request,"application/json");return httpd_resp_sendstr(request,json);
  }
  static esp_err_t pauseRoute(httpd_req_t* r){return operationRoute(r,SessionOperation::Pause);}
  static esp_err_t honourRoute(httpd_req_t* r){return operationRoute(r,SessionOperation::HonourRestart);}
  static esp_err_t gridRoute(httpd_req_t* r){return operationRoute(r,SessionOperation::GridRestart);}
  static esp_err_t raceAgainRoute(httpd_req_t* r){return operationRoute(r,SessionOperation::RaceAgain);}
  static esp_err_t resumeRoute(httpd_req_t* r){return operationRoute(r,SessionOperation::Resume);}
  static esp_err_t endSessionRoute(httpd_req_t* r){return operationRoute(r,SessionOperation::EndSession);}
  static esp_err_t openConfirmationRoute(httpd_req_t* request,SessionOperation operation){
    uint32_t ignored=0;if(!correlationFromBody(request,ignored)){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"correlationId required");return ESP_FAIL;}
    BrowserInterface* browser=instance();const uint64_t owner=client(request);
    if(browser->context(owner)!=ClientContext::RaceDirectorSmug){httpd_resp_send_err(request,HTTPD_403_FORBIDDEN,"Race Director required");return ESP_FAIL;}
    browser->confirmation_.owner=owner;browser->confirmation_.operation=operation;browser->confirmation_.open=true;
    httpd_resp_set_status(request,"202 Accepted");httpd_resp_set_type(request,"application/json");return httpd_resp_sendstr(request,"{\"confirmationRequired\":true}");
  }
  static esp_err_t confirmOperationRoute(httpd_req_t* request,SessionOperation operation){
    BrowserInterface* browser=instance();const uint64_t owner=client(request);
    if(!browser->confirmation_.open||browser->confirmation_.owner!=owner||browser->confirmation_.operation!=operation){httpd_resp_set_status(request,"409 Conflict");return httpd_resp_sendstr(request,"no matching confirmation");}
    browser->confirmation_.open=false;return operationRoute(request,operation,true);
  }
  static esp_err_t restartRaceRoute(httpd_req_t* r){return openConfirmationRoute(r,SessionOperation::RestartRace);}
  static esp_err_t endRaceRoute(httpd_req_t* r){return openConfirmationRoute(r,SessionOperation::EndRace);}
  static esp_err_t restartRaceConfirmRoute(httpd_req_t* r){return confirmOperationRoute(r,SessionOperation::RestartRace);}
  static esp_err_t endRaceConfirmRoute(httpd_req_t* r){return confirmOperationRoute(r,SessionOperation::EndRace);}
  static esp_err_t clearHistoryRoute(httpd_req_t* r){return openConfirmationRoute(r,SessionOperation::ClearHistory);}
  static esp_err_t clearHistoryConfirmRoute(httpd_req_t* r){return confirmOperationRoute(r,SessionOperation::ClearHistory);}
  static esp_err_t clearLane1RecordsRoute(httpd_req_t* r){return openConfirmationRoute(r,SessionOperation::ClearLane1Records);}
  static esp_err_t clearLane1RecordsConfirmRoute(httpd_req_t* r){return confirmOperationRoute(r,SessionOperation::ClearLane1Records);}
  static esp_err_t clearLane2RecordsRoute(httpd_req_t* r){return openConfirmationRoute(r,SessionOperation::ClearLane2Records);}
  static esp_err_t clearLane2RecordsConfirmRoute(httpd_req_t* r){return confirmOperationRoute(r,SessionOperation::ClearLane2Records);}
  static esp_err_t clearTrackRecordRoute(httpd_req_t* r){return openConfirmationRoute(r,SessionOperation::ClearTrackRecord);}
  static esp_err_t clearTrackRecordConfirmRoute(httpd_req_t* r){return confirmOperationRoute(r,SessionOperation::ClearTrackRecord);}
  static esp_err_t clearAllRecordsRoute(httpd_req_t* r){return openConfirmationRoute(r,SessionOperation::ClearAllRecords);}
  static esp_err_t clearAllRecordsConfirmRoute(httpd_req_t* r){return confirmOperationRoute(r,SessionOperation::ClearAllRecords);}
  static esp_err_t fixtureRoute(httpd_req_t* request){
#if defined(PP_STAGE11_DEMO) || defined(PP_STAGE11_ACCEPTANCE) || defined(PP_STAGE12_DEMO) || defined(PP_STAGE12_ACCEPTANCE) || defined(PP_STAGE13_DEMO) || defined(PP_STAGE13_ACCEPTANCE) || defined(PP_STAGE14A_DEMO) || defined(PP_STAGE14A_ACCEPTANCE)
    char body[64]{}; int n=httpd_req_recv(request,body,sizeof(body)-1); if(n<=0){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"fixture action required");return ESP_FAIL;} body[n]=0;
    if(instance()->context(client(request))!=ClientContext::RaceDirectorSmug){httpd_resp_send_err(request,HTTPD_403_FORBIDDEN,"Race Director required");return ESP_FAIL;}
    if(strstr(body,"lane1")&&instance()->fixturePass_){if(!instance()->fixturePass_(1)){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"Start the race before triggering a simulated car.");return ESP_FAIL;}}
    else if(strstr(body,"lane2")&&instance()->fixturePass_){if(!instance()->fixturePass_(2)){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"Start the race before triggering a simulated car.");return ESP_FAIL;}}
    else if(strstr(body,"reset")&&instance()->fixtureReset_) instance()->fixtureReset_();
    else if(strstr(body,"target3")&&instance()->fixtureSetup_){if(!instance()->fixtureSetup_(3)){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"Race Again must be accepted before editing the next race.");return ESP_FAIL;}}
    else {httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"unknown fixture action");return ESP_FAIL;}
    httpd_resp_set_type(request,"application/json"); return httpd_resp_sendstr(request,"{\"accepted\":true}");
#else
    httpd_resp_send_err(request,HTTPD_404_NOT_FOUND,"fixture unavailable"); return ESP_FAIL;
#endif
  }
  static esp_err_t resultRoute(httpd_req_t* request) {
    const uint64_t owner = client(request);
    char query[64]{};
    char rawCorrelation[16]{};
    if (httpd_req_get_url_query_str(request, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "correlationId", rawCorrelation, sizeof(rawCorrelation)) != ESP_OK) {
      httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "correlationId required");
      return ESP_FAIL;
    }
    char* end = nullptr;
    const unsigned long raw = strtoul(rawCorrelation, &end, 10);
    if (!end || *end || raw == 0 || raw > UINT32_MAX) {
      httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid correlationId");
      return ESP_FAIL;
    }
    ResultView result{};
    if (!instance()->requestResult(uint32_t(raw), owner, result)) {
      httpd_resp_set_status(request, "204 No Content");
      return httpd_resp_send(request, nullptr, 0);
    }
    char json[192];
    if (result.result == RequestResult::Accepted) {
      snprintf(json, sizeof(json), "{\"correlationId\":%lu,\"result\":\"ACCEPTED\"}", raw);
    } else {
      snprintf(json, sizeof(json),
        "{\"correlationId\":%lu,\"result\":\"REJECTED\",\"reason\":\"%s\"}",
        raw, rejectionText(result.rejection));
    }
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, json);
  }
  static esp_err_t page(httpd_req_t* request) {
    client(request);
    static const char html[] = R"HTML(<!doctype html><meta charset="utf-8"><title>P&amp;P diagnostic Browser</title><style>body{font:16px system-ui;max-width:52rem;margin:2rem auto;padding:0 1rem}pre{background:#eee;padding:1rem;white-space:pre-wrap}.summary{background:#f5f5f5;padding:1rem;border-radius:.35rem;line-height:1.5}.status{font-size:1.3rem;font-weight:600;margin:.5rem 0}button{font:inherit;padding:.5rem;margin:.15rem}.muted{color:#555}</style><h1>P&amp;P diagnostic Browser</h1><p id="connection">connecting</p><p id="role"></p><div id="human" class="summary">Loading race status...</div><p id="request"></p><button id="bootstrap">Make this Browser Race Director</button><button id="start">START</button><button id="pause">PAUSE</button><button id="honour">HONOUR RESTART</button><button id="grid">GRID RESTART</button><button id="raceAgain">RACE AGAIN</button><button id="restartRace">RESTART RACE</button><button id="endRace">END RACE</button><button id="results">RESULTS</button><button id="details">DETAILS</button><button id="history">HISTORY</button><button id="back">BACK</button><button id="home">HOME</button><button id="target3">NEXT RACE: 3 LAPS</button><div id="resultView" class="summary" hidden></div><h2>SIMULATED DETECTORS</h2><button id="lane1">LANE 1 PASS</button><button id="lane2">LANE 2 PASS</button><button id="resetTest">RESET TEST</button><h2>Engineering State</h2><pre id="state"></pre><h2>Latest Fact</h2><pre id="fact"></pre><script>let revision=null,hasState=false,nextCorrelation=1,polling=false,lastState=null;const $=x=>document.querySelector(x);async function get(path){const r=await fetch(path,{cache:'no-store'});if(r.status===204)return null;if(!r.ok)throw Error(r.status);return r.json()}function sec(us){return (Number(us||0)/1000000).toFixed(2)+' s'}function life(v){return ({READY:'Ready',STARTING:'Starting',RACING:'Racing',PAUSED:'Paused',RESTARTING:'Restarting',FINISHED:'Finished',FAULTED:'Faulted'})[v]||v}function meth(v){return v==='HONOUR'?'Honour Restart':v==='GRID'?'Grid Restart':'None'}function render(){const v=lastState;if(!v)return;let h='<div class="status">'+life(v.lifecycle)+'</div>';if(v.lifecycle==='PAUSED')h+='<div>Paused at the authoritative P&amp;P time.</div>';if(v.lifecycle==='RESTARTING')h+='<div>'+meth(v.restartMethod)+' - restart countdown active.</div>';if(v.lifecycle==='RACING')h+=v.sessionMode==='OPEN_PRACTICE'?'<div>Open Practice is running  -  no official result.</div>':'<div>Race is running.</div>'; if(v.lifecycle==='FINISHED')h+='<div>Race finished.</div>';h+='<div>'+v.entries.map((e,i)=>'<b>Lane '+e.lane+':</b> '+(e.laps||0)+' laps').join('; ')+'.</div>';if(v.hasLap)h+='<div>Last completed lap: '+sec(v.lastLapTime)+'</div>';h+='<div class="muted">Race integrity: '+(v.raceIntegrity==='OK'?'OK':'FAULTED')+'; results: '+(v.resultValid?'valid':'invalid')+'</div>';if(v.resultSealed)h+='<div><b>Winning time:</b> '+sec(v.winningTime)+'; <b>finish settled:</b> '+sec(v.finishTime)+'; <b>fastest lap:</b> '+(v.fastestLap?sec(v.fastestLap):'—')+'</div>';if(v.scheduledRestartAt&&v.lifecycle==='RESTARTING')h+='<div id="countdown" class="status">Restart countdown</div>';$('#human').innerHTML=h;if(v.lifecycle==='RESTARTING'&&v.scheduledRestartAt){const deadline=performance.now()+3000;clearInterval(window.restartTimer);window.restartTimer=setInterval(()=>{const c=$('#countdown');if(!c||!lastState||lastState.lifecycle!=='RESTARTING'){clearInterval(window.restartTimer);return}const left=Math.max(0,(deadline-performance.now())/1000);c.textContent=left>0?'Restarting - '+left.toFixed(1)+' s remaining':'Restarting - GO'},100)}}async function poll(){if(polling)return;polling=true;try{const n=await get('/noticeboard');if(!hasState||n.revision!==revision){$('#connection').textContent='unsynchronised';lastState=await get('/state');$('#state').textContent=JSON.stringify(lastState,null,2);revision=n.revision;hasState=true;try{$('#fact').textContent=JSON.stringify(await get('/fact'),null,2)}catch(e){}render()}$('#connection').textContent='synchronised';const c=await get('/context');$('#role').textContent=c.hasMaster?c.role:'No Race Director';$('#bootstrap').hidden=c.hasMaster;['start','pause','honour','grid'].forEach(id=>$('#'+id).disabled=c.role!=='Race Director');$('#raceAgain').disabled=c.role!=='Race Director'||!lastState||lastState.lifecycle!=='FINISHED';$('#target3').disabled=c.role!=='Race Director'||!lastState||lastState.lifecycle!=='READY'}catch(e){$('#connection').textContent='unsynchronised'}finally{polling=false}}async function operation(path,name){const id=nextCorrelation++,started=performance.now();const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({correlationId:id})});if(r.status!==202){$('#request').textContent='Request '+name+' was not accepted for submission.';return}const timer=setInterval(async()=>{const x=await get('/request-result?correlationId='+id);if(x){$('#request').textContent=(x.result==='ACCEPTED'?'Accepted: ':'Rejected: ')+(x.reason||'')+' ('+((performance.now()-started)/1000).toFixed(2)+' s; correlation '+id+')';clearInterval(timer)}},100)}$('#bootstrap').onclick=async()=>{await fetch('/bootstrap',{method:'POST'});poll()};$('#start').onclick=()=>operation('/request/start','START');$('#pause').onclick=()=>operation('/request/pause','PAUSE');$('#honour').onclick=()=>operation('/request/honour-restart','Honour Restart');$('#grid').onclick=()=>operation('/request/grid-restart','Grid Restart');$('#raceAgain').onclick=()=>operation('/request/race-again','Race Again');function rank(e){return(e.tied?'=':'')+e.rank}async function showResults(){const r=await get('/results');let h='<h2>Results</h2>';if(!r.sealed){h+='<p>No completed result.</p>'}else{h+='<p>Winning time '+sec(r.winningTime)+'; finish settled '+sec(r.finishTime)+'.</p>';h+='<p>Fastest lap: '+(r.fastestLap?sec(r.fastestLap)+(r.fastestLapTied?' (tie)':''):'—')+'</p>';r.entries.forEach((e,i)=>h+='<p><b>'+rank(e)+' — Lane '+e.lane+'</b>: '+e.laps+' laps'+(e.lapsBehind?' ('+e.lapsBehind+' behind)':'')+'; finish '+(e.completionTime?sec(e.completionTime):'—')+'; best lap '+(e.bestLap?sec(e.bestLap):'—')+'</p>')}$('#resultView').innerHTML=h;$('#resultView').hidden=false}async function showDetails(){const r=await get('/details');let h='<h2>Lap details</h2>';r.entries.forEach((e,i)=>{h+='<h3>Lane '+e.lane+' — entry '+e.raceEntryId+'</h3>';h+=e.records.length?'<ol>'+e.records.map(x=>'<li>Lap '+x.lapNumber+': '+sec(x.lapTime)+'</li>').join('')+'</ol>':'<p>No valid completed laps.</p>'});$('#resultView').innerHTML=h;$('#resultView').hidden=false}$('#results').onclick=showResults;$('#details').onclick=showDetails;$('#back').onclick=()=>{$('#resultView').hidden=true};$('#home').onclick=()=>{$('#resultView').hidden=true;$('#request').textContent='Home — race result remains available.'};async function fixture(action,name){const r=await fetch('/fixture',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action})});$('#request').textContent=r.ok?(action==='lane1'?'Lane 1 car triggered':action==='lane2'?'Lane 2 car triggered':action==='target3'?'Next race set to 3 laps.':'TEST RESET complete'):(action==='lane1'||action==='lane2'?'Start the race before triggering a simulated car.':name+' unavailable')}$('#lane1').onclick=()=>fixture('lane1','Lane 1 passage');$('#lane2').onclick=()=>fixture('lane2','Lane 2 passage');$('#resetTest').onclick=()=>fixture('reset','TEST RESET');$('#target3').onclick=()=>fixture('target3','Next-race setup');poll();setInterval(poll,250)</script>)HTML";
    static const char destructiveControls[] = R"HTML(<script>async function destructive(open,confirmPath,label){const id=Date.now()%4294967295;const opened=await fetch(open,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({correlationId:id})});if(!opened.ok){document.querySelector('#request').textContent=label+' is unavailable.';return}if(!confirm('Confirm '+label+'?')){document.querySelector('#request').textContent=label+' cancelled.';return}const r=await fetch(confirmPath,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({correlationId:id})});document.querySelector('#request').textContent=r.status===202?label+' submitted.':label+' was not accepted for submission.'}document.querySelector('#restartRace').onclick=()=>destructive('/request/restart-race','/request/restart-race/confirm','Restart Race');document.querySelector('#endRace').onclick=()=>destructive('/request/end-race','/request/end-race/confirm','End Race');document.querySelector('#history').onclick=async()=>{const r=await fetch('/history');const h=await r.json();const v=document.querySelector('#resultView');v.innerHTML='<h2>History</h2>'+(h.entries.length?h.entries.map(x=>'<p>Race '+x.sequence+': winning time '+(Number(x.winningTime)/1000000).toFixed(2)+' s</p>').join(''):'<p>No completed races.</p>');v.hidden=false};async function stage13Fact(){try{const f=await get('/fact');const old=document.querySelector('#stage13Fact');if(old)old.remove();if(f.type==='FALSE_START'){const msg=document.createElement('div');msg.id='stage13Fact';msg.className='status';msg.textContent='False start — Lane '+f.raceEntryId+' ('+f.policy+').';document.querySelector('#human').appendChild(msg)}}catch(e){}}function stage13Lights(){try{if(!lastState||lastState.lifecycle!=='STARTING'||!lastState.systemTime)return;const now=Number(lastState.systemTime)+(performance.now()-(lastState.sampledAt||performance.now()))*1000;const reds=Number(lastState.redLightCount||0),gap=Number(lastState.redIntervalUs||0),finalDelay=Number(lastState.finalDelayUs||0),first=Number(lastState.scheduledGo)-reds*gap-finalDelay;const lit=Math.max(0,Math.min(reds,Math.floor((now-first)/gap)+1));let line=document.querySelector('#stage13Lights');if(!line){line=document.createElement('div');line.id='stage13Lights';line.className='status';document.querySelector('#human').appendChild(line)}line.textContent=now>=Number(lastState.scheduledGo)?(lastState.startSignal===1?'GO — GREEN':'GO — LIGHTS OUT'):('Start sequence — '+lit+' of '+reds+' red lights');}catch(e){}}setInterval(stage13Lights,100);setInterval(stage13Fact,300);</script>)HTML";
    static const char recordControls[] = R"HTML(<script>const recordButton=document.createElement('button');recordButton.id='records';recordButton.textContent='RECORDS';document.querySelector('#history').after(recordButton);function recordTime(v){return v?sec(v):'—'}async function showRecords(){const r=await get('/records');const v=document.querySelector('#resultView');v.innerHTML='<h2>Records — '+r.track+'</h2><p>Record era '+r.era+'. '+r.entries.map(e=>'Lane '+e.lane+' PB: '+recordTime(e.pb)).join('; ')+ '; Track Record: '+recordTime(r.trackRecord)+'.</p><p><button id="clearHistory">CLEAR HISTORY</button><button id="clearLane1">CLEAR LANE 1 PB</button><button id="clearLane2">CLEAR LANE 2 PB</button><button id="clearTrack">CLEAR TRACK RECORD</button><button id="clearAll">CLEAR ALL RECORDS</button></p>';v.hidden=false;document.querySelector('#clearHistory').onclick=()=>destructive('/request/clear-history','/request/clear-history/confirm','Clear History');document.querySelector('#clearLane1').onclick=()=>destructive('/request/clear-lane-1-records','/request/clear-lane-1-records/confirm','Clear Lane 1 PB');document.querySelector('#clearLane2').onclick=()=>destructive('/request/clear-lane-2-records','/request/clear-lane-2-records/confirm','Clear Lane 2 PB');document.querySelector('#clearTrack').onclick=()=>destructive('/request/clear-track-record','/request/clear-track-record/confirm','Clear Track Record');document.querySelector('#clearAll').onclick=()=>destructive('/request/clear-all-records','/request/clear-all-records/confirm','Clear All Records')}recordButton.onclick=showRecords;</script>)HTML";
    // The sampled browser monotonic instant is presentation-only.  It lets a
    // reconnecting Browser animate the already-authoritative schedule without
    // deciding GO or sending a timing value back to P&P.
    static const char stage13Presentation[] = R"HTML(<script>setInterval(()=>{try{if(lastState&&!lastState.sampledAt)lastState.sampledAt=performance.now()}catch(e){}},25);</script>)HTML";
    static const char stage14bPresentation[] = R"HTML(<script>(function(){const start=document.querySelector('#start');const pause=document.querySelector('#pause');const raceAgain=document.querySelector('#raceAgain');const target=document.querySelector('#target3');const lane1=document.querySelector('#lane1');const lane2=document.querySelector('#lane2');const reset=document.querySelector('#resetTest');const controls=document.createElement('div');controls.id='practiceControls';controls.innerHTML='<h2>OPEN PRACTICE</h2><button id="practiceSelect">SELECT OPEN PRACTICE</button><button id="practiceResume">RESUME</button><button id="practiceEnd">END SESSION</button>';document.querySelector('#bootstrap').before(controls);let practiceSelected=false;const select=document.querySelector('#practiceSelect');const resume=document.querySelector('#practiceResume');const end=document.querySelector('#practiceEnd');select.onclick=()=>{practiceSelected=true;select.textContent='OPEN PRACTICE SELECTED';if(start)start.disabled=false};resume.onclick=()=>operation('/request/resume','RESUME');end.onclick=()=>operation('/request/end-session','END SESSION');const oldStart=start?start.onclick:null;if(start)start.onclick=()=>{if(!practiceSelected){if(oldStart)oldStart();return}const id=nextCorrelation++,started=performance.now();fetch('/request/start',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({correlationId:id,mode:'OPEN_PRACTICE'})}).then(async r=>{if(r.status!==202){$('#request').textContent='Request START was not accepted for submission.';return}const timer=setInterval(async()=>{const x=await get('/request-result?correlationId='+id);if(x){$('#request').textContent=(x.result==='ACCEPTED'?'Accepted: ':'Rejected: ')+(x.reason||'')+' ('+((performance.now()-started)/1000).toFixed(2)+' s; correlation '+id+')';clearInterval(timer)}},100)});};const oldPoll=poll;window.poll=async function(){await oldPoll();const v=lastState;const isPractice=!!v&&v.sessionMode==='OPEN_PRACTICE';if(select)select.disabled=!!v&&v.lifecycle!=='READY';if(resume)resume.disabled=!isPractice||!v||v.lifecycle!=='PAUSED';if(end)end.disabled=!isPractice||!v||(['RACING','PAUSED'].indexOf(v.lifecycle)<0);if(start&&!isPractice)start.disabled=false;if(pause)pause.disabled=!isPractice&&pause.disabled;[lane1,lane2].forEach(x=>{if(x)x.disabled=!isPractice&&x.disabled});};})();</script>)HTML";
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    if (httpd_resp_send_chunk(request, html, sizeof(html) - 1) != ESP_OK) return ESP_FAIL;
    if (httpd_resp_send_chunk(request, destructiveControls, sizeof(destructiveControls) - 1) != ESP_OK) return ESP_FAIL;
    if (httpd_resp_send_chunk(request, recordControls, sizeof(recordControls) - 1) != ESP_OK) return ESP_FAIL;
    if (httpd_resp_send_chunk(request, stage13Presentation, sizeof(stage13Presentation) - 1) != ESP_OK) return ESP_FAIL;
    if (httpd_resp_send_chunk(request, stage14bPresentation, sizeof(stage14bPresentation) - 1) != ESP_OK) return ESP_FAIL;
    return httpd_resp_send_chunk(request, nullptr, 0);
  }
  static esp_err_t health(httpd_req_t* request) {
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, "{\"ok\":true,\"browser\":\"stage13\"}");
  }
  void startServer() {
    instance() = this;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 8192;
    // ESP32 Arduino's configured LWIP limit permits at most 13 sockets.
    // LRU eviction keeps sleeping presentation clients from exhausting them.
    config.max_open_sockets = 13;
    config.lru_purge_enable = true;
    config.max_uri_handlers = 32;
    const esp_err_t started = httpd_start(&server_, &config);
    if (started != ESP_OK) {
      serverStartError_ = int(started);
      server_ = nullptr;
      return;
    }
    httpd_uri_t routes[] = {
      {"/", HTTP_GET, page, nullptr},
      {"/state", HTTP_GET, state, nullptr},
      {"/noticeboard", HTTP_GET, notice, nullptr},
      {"/results", HTTP_GET, resultsRoute, nullptr},
      {"/details", HTTP_GET, detailsRoute, nullptr},
      {"/history", HTTP_GET, historyRoute, nullptr},
      {"/records", HTTP_GET, recordsRoute, nullptr},
      {"/fact", HTTP_GET, fact, nullptr},
      {"/health", HTTP_GET, health, nullptr},
      {"/context", HTTP_GET, contextRoute, nullptr},
      {"/bootstrap", HTTP_POST, bootstrapRoute, nullptr},
      {"/request/start", HTTP_POST, startRoute, nullptr},
      {"/request/pause", HTTP_POST, pauseRoute, nullptr},
      {"/request/resume", HTTP_POST, resumeRoute, nullptr},
      {"/request/end-session", HTTP_POST, endSessionRoute, nullptr},
      {"/request/honour-restart", HTTP_POST, honourRoute, nullptr},
      {"/request/grid-restart", HTTP_POST, gridRoute, nullptr},
      {"/request/race-again", HTTP_POST, raceAgainRoute, nullptr},
      {"/request/restart-race", HTTP_POST, restartRaceRoute, nullptr},
      {"/request/restart-race/confirm", HTTP_POST, restartRaceConfirmRoute, nullptr},
      {"/request/end-race", HTTP_POST, endRaceRoute, nullptr},
      {"/request/end-race/confirm", HTTP_POST, endRaceConfirmRoute, nullptr},
      {"/request/clear-history", HTTP_POST, clearHistoryRoute, nullptr},
      {"/request/clear-history/confirm", HTTP_POST, clearHistoryConfirmRoute, nullptr},
      {"/request/clear-lane-1-records", HTTP_POST, clearLane1RecordsRoute, nullptr},
      {"/request/clear-lane-1-records/confirm", HTTP_POST, clearLane1RecordsConfirmRoute, nullptr},
      {"/request/clear-lane-2-records", HTTP_POST, clearLane2RecordsRoute, nullptr},
      {"/request/clear-lane-2-records/confirm", HTTP_POST, clearLane2RecordsConfirmRoute, nullptr},
      {"/request/clear-track-record", HTTP_POST, clearTrackRecordRoute, nullptr},
      {"/request/clear-track-record/confirm", HTTP_POST, clearTrackRecordConfirmRoute, nullptr},
      {"/request/clear-all-records", HTTP_POST, clearAllRecordsRoute, nullptr},
      {"/request/clear-all-records/confirm", HTTP_POST, clearAllRecordsConfirmRoute, nullptr},
      {"/fixture", HTTP_POST, fixtureRoute, nullptr},
      {"/request-result", HTTP_GET, resultRoute, nullptr},
    };
    for (httpd_uri_t& route : routes) {
      const esp_err_t registered = httpd_register_uri_handler(server_, &route);
      if (registered != ESP_OK) {
        serverStartError_ = int(registered);
        httpd_stop(server_);
        server_ = nullptr;
        return;
      }
    }
  }
};
}  // namespace pp
