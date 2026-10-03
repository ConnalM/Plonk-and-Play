#pragma once

#include <WiFi.h>
#include <esp_http_server.h>
#include <esp_system.h>
#include <stdlib.h>

#include "noticeboard.h"

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
                   AuthorityStore* authority = nullptr)
      : bus_(bus), endpoint_(endpoint), noticeboard_(noticeboard),
        authority_(authority ? authority : &fallback_) {
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
      } else if (message.type == Type::Paused || message.type == Type::RestartScheduled || message.type == Type::Resumed) {
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

  bool submitStart(uint32_t visibleCorrelation, ClientContext context, uint64_t owner) {
    if (!visibleCorrelation) return false;
    Message request{};
    request.type = Type::StartRequest;
    request.correlation = nextInternalCorrelation();
    request.clientContext = context;
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
  void setFixtureCallbacks(bool (*pass)(uint8_t), void (*reset)()){fixturePass_=pass;fixtureReset_=reset;}
  bool submitOperation(uint32_t visibleCorrelation, SessionOperation operation, ClientContext context, uint64_t owner) {
    if (!visibleCorrelation) return false;
    Message request{}; request.type=Type::SessionOperationRequest; request.correlation=nextInternalCorrelation(); request.operation=operation; request.clientContext=context;
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

  static BrowserInterface*& instance() {
    static BrowserInterface* value = nullptr;
    return value;
  }

  Bus& bus_;
  Bus::Endpoint endpoint_;
  const Noticeboard& noticeboard_;
  AuthorityStore* authority_;
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
  bool (*fixturePass_)(uint8_t) = nullptr;
  void (*fixtureReset_)() = nullptr;

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
      case RequestRejection::InvalidRaceSetup: return "invalid race setup: lap target must be at least 1";
      case RequestRejection::RequiredCapabilityUnavailable: return "required Start/Finish capability unavailable";
      case RequestRejection::SessionDefinitionUnavailable: return "unable to establish Session Definition";
      case RequestRejection::LifecycleNotPausable: return "PAUSE is not valid in the current lifecycle state";
      case RequestRejection::LifecycleNotRestartable: return "restart is not valid in the current lifecycle state";
      case RequestRejection::PauseSettlementPending: return "PAUSE settlement is still pending";
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

  static esp_err_t state(httpd_req_t* request) {
    client(request);
    const NoticeboardState value = instance()->current();
    char json[600];
    snprintf(json, sizeof(json),
      "{\"lifecycle\":\"%s\",\"raceEntryId\":%lu,\"laps\":%lu,\"hasLap\":%s,\"lastLapTime\":%llu,\"scheduledGo\":%llu,\"pauseEffectiveAt\":%llu,\"scheduledRestartAt\":%llu,\"restartMethod\":\"%s\",\"resultValid\":%s,\"raceIntegrity\":\"%s\",\"entries\":[{\"raceEntryId\":%lu,\"laps\":%lu},{\"raceEntryId\":%lu,\"laps\":%lu}]}",
      lifecycle(value.lifecycle), (unsigned long)value.raceEntryId, (unsigned long)value.laps,
      value.hasLap ? "true" : "false", (unsigned long long)value.lastLapTime,
      (unsigned long long)value.scheduledGo,(unsigned long long)value.pauseEffectiveAt,(unsigned long long)value.scheduledRestartAt,
      value.restartMethod==RestartMethod::Honour?"HONOUR":value.restartMethod==RestartMethod::Grid?"GRID":"NONE", value.resultValid ? "true" : "false",
      value.raceIntegrityFaulted ? "FAULTED" : "OK",
      (unsigned long)value.entries[0].raceEntryId, (unsigned long)value.entries[0].laps,
      (unsigned long)value.entries[1].raceEntryId, (unsigned long)value.entries[1].laps);
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, json);
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
    } else {
      const char* name=fact.type==Type::Paused?"PAUSED":fact.type==Type::RestartScheduled?"RESTART_SCHEDULED":"RESUMED";
      snprintf(json,sizeof(json),"{\"type\":\"%s\",\"revision\":%lu,\"relevantTime\":%llu}",name,(unsigned long)instance()->factRevision(),(unsigned long long)fact.relevantTime);
    }
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, json);
  }

  static bool correlationFromBody(httpd_req_t* request, uint32_t& correlation) {
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
    if (!correlationFromBody(request, correlation)) {
      httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "correlationId required");
      return ESP_FAIL;
    }
    const uint64_t owner = client(request);
    if (!instance()->submitStart(correlation, instance()->context(owner), owner)) {
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

  static esp_err_t operationRoute(httpd_req_t* request, SessionOperation operation) {
    uint32_t correlation=0;if(!correlationFromBody(request,correlation)){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"correlationId required");return ESP_FAIL;}
    const uint64_t owner=client(request);
    if(!instance()->submitOperation(correlation,operation,instance()->context(owner),owner)){httpd_resp_set_status(request,"503 Service Unavailable");return httpd_resp_sendstr(request,"request unavailable");}
    char json[96];snprintf(json,sizeof(json),"{\"submitted\":true,\"correlationId\":%lu}",(unsigned long)correlation);httpd_resp_set_status(request,"202 Accepted");httpd_resp_set_type(request,"application/json");return httpd_resp_sendstr(request,json);
  }
  static esp_err_t pauseRoute(httpd_req_t* r){return operationRoute(r,SessionOperation::Pause);}
  static esp_err_t honourRoute(httpd_req_t* r){return operationRoute(r,SessionOperation::HonourRestart);}
  static esp_err_t gridRoute(httpd_req_t* r){return operationRoute(r,SessionOperation::GridRestart);}
  static esp_err_t fixtureRoute(httpd_req_t* request){
#if defined(PP_STAGE11_DEMO) || defined(PP_STAGE11_ACCEPTANCE)
    char body[64]{}; int n=httpd_req_recv(request,body,sizeof(body)-1); if(n<=0){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"fixture action required");return ESP_FAIL;} body[n]=0;
    if(instance()->context(client(request))!=ClientContext::RaceDirectorSmug){httpd_resp_send_err(request,HTTPD_403_FORBIDDEN,"Race Director required");return ESP_FAIL;}
    if(strstr(body,"lane1")&&instance()->fixturePass_){if(!instance()->fixturePass_(1)){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"Start the race before triggering a simulated car.");return ESP_FAIL;}}
    else if(strstr(body,"lane2")&&instance()->fixturePass_){if(!instance()->fixturePass_(2)){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"Start the race before triggering a simulated car.");return ESP_FAIL;}}
    else if(strstr(body,"reset")&&instance()->fixtureReset_) instance()->fixtureReset_();
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
    static const char html[] = R"HTML(<!doctype html><meta charset="utf-8"><title>P&amp;P diagnostic Browser</title><style>body{font:16px system-ui;max-width:52rem;margin:2rem auto;padding:0 1rem}pre{background:#eee;padding:1rem;white-space:pre-wrap}.summary{background:#f5f5f5;padding:1rem;border-radius:.35rem;line-height:1.5}.status{font-size:1.3rem;font-weight:600;margin:.5rem 0}button{font:inherit;padding:.5rem;margin:.15rem}.muted{color:#555}</style><h1>P&amp;P diagnostic Browser</h1><p id="connection">connecting</p><p id="role"></p><div id="human" class="summary">Loading race status...</div><p id="request"></p><button id="bootstrap">Make this Browser Race Director</button><button id="start">START</button><button id="pause">PAUSE</button><button id="honour">HONOUR RESTART</button><button id="grid">GRID RESTART</button><h2>SIMULATED DETECTORS</h2><button id="lane1">LANE 1 PASS</button><button id="lane2">LANE 2 PASS</button><button id="resetTest">RESET TEST</button><h2>Engineering State</h2><pre id="state"></pre><h2>Latest Fact</h2><pre id="fact"></pre><script>let revision=null,hasState=false,nextCorrelation=1,polling=false,lastState=null;const $=x=>document.querySelector(x);async function get(path){const r=await fetch(path,{cache:'no-store'});if(r.status===204)return null;if(!r.ok)throw Error(r.status);return r.json()}function sec(us){return (Number(us||0)/1000000).toFixed(2)+' s'}function life(v){return ({READY:'Ready',STARTING:'Starting',RACING:'Racing',PAUSED:'Paused',RESTARTING:'Restarting',FINISHED:'Finished',FAULTED:'Faulted'})[v]||v}function meth(v){return v==='HONOUR'?'Honour Restart':v==='GRID'?'Grid Restart':'None'}function render(){const v=lastState;if(!v)return;let h='<div class="status">'+life(v.lifecycle)+'</div>';if(v.lifecycle==='PAUSED')h+='<div>Paused at the authoritative P&amp;P time.</div>';if(v.lifecycle==='RESTARTING')h+='<div>'+meth(v.restartMethod)+' - restart countdown active.</div>';if(v.lifecycle==='RACING')h+='<div>Race is running.</div>';if(v.lifecycle==='FINISHED')h+='<div>Race finished.</div>';h+='<div><b>Lane 1:</b> '+(v.entries[0]?.laps||0)+' laps; <b>Lane 2:</b> '+(v.entries[1]?.laps||0)+' laps.</div>';if(v.hasLap)h+='<div>Last completed lap: '+sec(v.lastLapTime)+'</div>';h+='<div class="muted">Race integrity: '+(v.raceIntegrity==='OK'?'OK':'FAULTED')+'; results: '+(v.resultValid?'valid':'invalid')+'</div>';if(v.scheduledRestartAt&&v.lifecycle==='RESTARTING')h+='<div id="countdown" class="status">Restart countdown</div>';$('#human').innerHTML=h;if(v.lifecycle==='RESTARTING'&&v.scheduledRestartAt){const deadline=performance.now()+3000;clearInterval(window.restartTimer);window.restartTimer=setInterval(()=>{const c=$('#countdown');if(!c||!lastState||lastState.lifecycle!=='RESTARTING'){clearInterval(window.restartTimer);return}const left=Math.max(0,(deadline-performance.now())/1000);c.textContent=left>0?'Restarting - '+left.toFixed(1)+' s remaining':'Restarting - GO'},100)}}async function poll(){if(polling)return;polling=true;try{const n=await get('/noticeboard');if(!hasState||n.revision!==revision){$('#connection').textContent='unsynchronised';lastState=await get('/state');$('#state').textContent=JSON.stringify(lastState,null,2);revision=n.revision;hasState=true;try{$('#fact').textContent=JSON.stringify(await get('/fact'),null,2)}catch(e){}render()}$('#connection').textContent='synchronised';const c=await get('/context');$('#role').textContent=c.hasMaster?c.role:'No Race Director';$('#bootstrap').hidden=c.hasMaster;['start','pause','honour','grid'].forEach(id=>$('#'+id).disabled=c.role!=='Race Director')}catch(e){$('#connection').textContent='unsynchronised'}finally{polling=false}}async function operation(path,name){const id=nextCorrelation++,started=performance.now();const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({correlationId:id})});if(r.status!==202){$('#request').textContent='Request '+name+' was not accepted for submission.';return}const timer=setInterval(async()=>{const x=await get('/request-result?correlationId='+id);if(x){$('#request').textContent=(x.result==='ACCEPTED'?'Accepted: ':'Rejected: ')+(x.reason||'')+' ('+((performance.now()-started)/1000).toFixed(2)+' s; correlation '+id+')';clearInterval(timer)}},100)}$('#bootstrap').onclick=async()=>{await fetch('/bootstrap',{method:'POST'});poll()};$('#start').onclick=()=>operation('/request/start','START');$('#pause').onclick=()=>operation('/request/pause','PAUSE');$('#honour').onclick=()=>operation('/request/honour-restart','Honour Restart');$('#grid').onclick=()=>operation('/request/grid-restart','Grid Restart');async function fixture(action,name){const r=await fetch('/fixture',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action})});$('#request').textContent=r.ok?(action==='lane1'?'Lane 1 car triggered':action==='lane2'?'Lane 2 car triggered':'TEST RESET complete'):(action==='lane1'||action==='lane2'?'Start the race before triggering a simulated car.':name+' unavailable')}$('#lane1').onclick=()=>fixture('lane1','Lane 1 passage');$('#lane2').onclick=()=>fixture('lane2','Lane 2 passage');$('#resetTest').onclick=()=>fixture('reset','TEST RESET');poll();setInterval(poll,250)</script>)HTML";
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, html, sizeof(html) - 1);
  }

  static esp_err_t health(httpd_req_t* request) {
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, "{\"ok\":true,\"browser\":\"stage10\"}");
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
    config.max_uri_handlers = 13;
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
      {"/fact", HTTP_GET, fact, nullptr},
      {"/health", HTTP_GET, health, nullptr},
      {"/context", HTTP_GET, contextRoute, nullptr},
      {"/bootstrap", HTTP_POST, bootstrapRoute, nullptr},
      {"/request/start", HTTP_POST, startRoute, nullptr},
      {"/request/pause", HTTP_POST, pauseRoute, nullptr},
      {"/request/honour-restart", HTTP_POST, honourRoute, nullptr},
      {"/request/grid-restart", HTTP_POST, gridRoute, nullptr},
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
