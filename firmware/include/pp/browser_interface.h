#pragma once
#include <WiFi.h>
#include <esp_http_server.h>
#include <esp_system.h>
#include <stdlib.h>
#include <stdarg.h>
#include "noticeboard.h"
#include "system_time.h"
#include "diagnostic_trace.h"
#include "diagnostic_retrieval.h"
#include "control_lease.h"
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
    controllerEpoch_ = newControllerEpoch();
    uint64_t persisted = 0;
    if (authority_->loadMaster(persisted) && persisted) controlLease_.restore(persisted);
  }
  uint64_t controllerEpoch() const { return controllerEpoch_; }
  void setDiagnosticTrace(diagnostic::ControllerOperationTrace* trace) { diagnosticTrace_ = trace; }
  void begin() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin("Wokwi-GUEST", "", 6);
    lastWifiAttemptMs_ = millis();
    wifiConnected_ = false;
    started_ = true;
  }
  void tick() {
    Message message;
    while (bus_.receive(endpoint_, message)) {
      if (message.type == Type::NoticeboardChanged) {
        ++noticeRevision_;
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
        if (lapNoticePending_) {
          lapNoticeRevision_ = noticeRevision_;
          lapNoticePending_ = false;
          Serial.printf("[SSE] notice_changed revision=%lu lap_entry=%lu lap=%lu event_id=%lu relevant_us=%llu at_us=%llu\n",
            static_cast<unsigned long>(noticeRevision_),
            static_cast<unsigned long>(lapNoticeEntry_),
            static_cast<unsigned long>(lapNoticeNumber_),
            static_cast<unsigned long>(lapNoticeEventId_),
            static_cast<unsigned long long>(lapNoticeRelevantUs_),
            static_cast<unsigned long long>(systemTime()));
        }
#endif
      } else if (message.type == Type::LapCompleted) {
        lastFact_ = message;
        hasFact_ = true;
        ++factRevision_;
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
        recordLapDisplayPass(message);
        lapNoticePending_ = true;
        lapNoticeEntry_ = message.raceEntryId;
        lapNoticeNumber_ = message.lapNumber;
        lapNoticeEventId_ = message.eventId;
        lapNoticeRelevantUs_ = message.relevantTime;
#endif
      } else if (message.type == Type::GoScheduled) {
        scheduledGo_ = message.relevantTime;
      } else if (message.type == Type::Paused || message.type == Type::RestartScheduled || message.type == Type::Resumed || message.type == Type::FalseStart || message.type == Type::HistoryStored || message.type == Type::StorageFault) {
        lastFact_ = message; hasFact_ = true; ++factRevision_;
      } else if (message.type == Type::RequestResult) {
        recordResult(message);
      }
    }
    if (!started_) return;
    const bool connected = WiFi.status() == WL_CONNECTED;
    const uint32_t now = millis();
    if (!connected) {
      wifiConnected_ = false;
      if (server_) stopServer();
      // The simulator/gateway can bring the development network up after
      // boot, or after a later link loss. Keep this bounded and non-blocking.
      if (static_cast<uint32_t>(now - lastWifiAttemptMs_) >= 5000U) {
        ++wifiReconnectAttempts_;
        WiFi.reconnect();
        lastWifiAttemptMs_ = now;
      }
      return;
    }
    wifiConnected_ = true;
    if (!server_) startServer();
    updatePresentation(systemTime());
    // Noticeboard changes are coalesced into one bounded SSE notification.
    // The Browser still reads authoritative state through the existing routes.
    if (server_ && eventConnected_ && eventGeneration_ != 0) {
      const bool changed = noticeRevision_ != eventSentRevision_;
      const bool presentationChanged = presentationSequence_ != presentationSentSequence_;
      const bool keepalive = static_cast<uint32_t>(now - eventLastSendMs_) >= 10000U;
      if ((changed || presentationChanged || controlNotificationPending_ || keepalive) && !eventWorkQueued_) queueEventWork();
    }
  }
  // The public overload is retained for non-Browser test fixtures. HTTP calls
  // always include their server-resolved owner token below.
  bool submitStart(uint32_t visibleCorrelation, ClientContext context) {
    return submitStart(visibleCorrelation, context, 0);
  }
  bool submitStart(uint32_t visibleCorrelation, ClientContext context, uint64_t owner, SessionMode mode=SessionMode::None, uint16_t durationMinutes=0, uint8_t finishPolicy=0, uint64_t traceId=0) {
    if (!visibleCorrelation) return false;
    Message request{};
    request.type = Type::StartRequest;
    request.correlation = nextInternalCorrelation();
    request.clientContext = context;
    // START is deliberately generic: Race Control commits the accepted P&P
    // proposal.  Browser-supplied setup values are never authoritative.
    (void)mode; (void)durationMinutes; (void)finishPolicy;
    request.sessionMode = SessionMode::None;
    request.durationMinutes = 0;
    request.finishPolicy = 0;
    if (diagnosticTrace_) { diagnostic::ClientId clientId{}; clientId.value = owner; diagnosticTrace_->observeInterfaceRequest(diagnostic::OperationName::Start, visibleCorrelation, request.correlation, clientId, traceId, systemTime()); }
    if (bus_.publish(endpoint_, request) != Delivery::Delivered) return false;
    Pending& pending = pending_[nextPending_++ % ResultCapacity];
    pending.internal = request.correlation;
    pending.visible = visibleCorrelation;
    pending.owner = owner;
    pending.ready = true;
    return true;
  }
  bool submitSetup(uint32_t visibleCorrelation, ClientContext context, uint64_t owner,
                   SessionMode mode, uint32_t lapTarget, uint16_t durationMinutes,
                   uint8_t finishPolicy, uint32_t proposalRevision, uint64_t traceId=0) {
    if (!visibleCorrelation) return false;
    Message request{}; request.type=Type::SetupRequest; request.correlation=nextInternalCorrelation();
    request.clientContext=context; request.sessionMode=mode; request.setupLapTarget=lapTarget;
    request.setupDurationMinutes=durationMinutes; request.setupFinishPolicy=finishPolicy;
    request.proposalRevision=proposalRevision;
    if (diagnosticTrace_) { diagnostic::ClientId clientId{}; clientId.value = owner; diagnosticTrace_->observeInterfaceRequest(diagnostic::OperationName::Setup, visibleCorrelation, request.correlation, clientId, traceId, systemTime()); }
    if(bus_.publish(endpoint_,request)!=Delivery::Delivered) return false;
    Pending& pending=pending_[nextPending_++ % ResultCapacity]; pending.internal=request.correlation;
    pending.visible=visibleCorrelation; pending.owner=owner; pending.ready=true; return true;
  }
  void clearForFixture(){controlLease_.reset();clearRaceForFixture();}
  void clearRaceForFixture(){for(auto& p:pending_)p.ready=false;for(auto& r:results_)r.ready=false;hasFact_=false;lastFact_={};noticeRevision_=0;factRevision_=0;scheduledGo_=0;}
  void setFixtureCallbacks(bool (*pass)(uint8_t), void (*reset)()){fixturePass_=pass;fixtureReset_=reset;}
  bool submitOperation(uint32_t visibleCorrelation, SessionOperation operation, ClientContext context, uint64_t owner, bool confirmed=false, uint64_t traceId=0) {
    if (!visibleCorrelation) return false;
    Message request{}; request.type=Type::SessionOperationRequest; request.correlation=nextInternalCorrelation(); request.operation=operation; request.clientContext=context;request.probe=confirmed?1:0;
    if (diagnosticTrace_) { diagnostic::ClientId clientId{}; clientId.value = owner; diagnosticTrace_->observeInterfaceRequest(diagnosticOperation(operation), visibleCorrelation, request.correlation, clientId, traceId, systemTime()); }
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
    if (!token || controlLease_.hasOwner()) return false;
    const uint64_t candidate = fingerprint(token);
    if (authority_->saveMaster(candidate) && controlLease_.restore(candidate)) return true;
    return false;
  }
  bool hasMaster() const { return controlLease_.hasOwner(); }
  ClientContext context(uint64_t token) const {
    return token && controlLease_.isOwner(fingerprint(token))
               ? ClientContext::RaceDirectorSmug
               : ClientContext::Spectator;
  }
  bool isControlOwner(uint64_t token) const { return token && controlLease_.isOwner(fingerprint(token)); }
  bool controlOwnerConnected() const { return controlLease_.ownerConnected(); }
  bool controlTakeoverPending() const { return controlLease_.pending(); }
  uint32_t controlTakeoverRequestId() const { return controlLease_.requestId(); }
  bool controlTakeoverRequestedBy(uint64_t token) const {
    return token && controlLease_.requester() == fingerprint(token);
  }
  ControlLeaseResult requestControl(uint64_t token, uint32_t& requestId) {
    const uint64_t candidate = fingerprint(token);
    const ControlLease::Snapshot before = controlLease_.snapshot();
    const ControlLeaseResult result = controlLease_.request(candidate, requestId);
    if (result == ControlLeaseResult::Accepted &&
        (!authority_->saveMaster(controlLease_.owner()))) {
      controlLease_.restoreSnapshot(before);
      requestId = 0;
      return ControlLeaseResult::NotOwner;
    }
    if (result == ControlLeaseResult::Pending) {
      controlNotificationRequestId_ = requestId;
      controlNotificationPending_ = true;
      if (server_ && eventConnected_) queueEventWork();
    }
    return result;
  }
  ControlLeaseResult respondControl(uint64_t token, bool allow, uint32_t& requestId) {
    const uint64_t candidate = fingerprint(token);
    const ControlLease::Snapshot before = controlLease_.snapshot();
    const ControlLeaseResult result = controlLease_.respond(candidate, allow, requestId);
    if (result == ControlLeaseResult::Accepted &&
        (!authority_->saveMaster(controlLease_.owner()))) {
      controlLease_.restoreSnapshot(before);
      return ControlLeaseResult::NotOwner;
    }
    return result;
  }
  void controlConnected(uint64_t token) { if (token) controlLease_.connected(fingerprint(token)); }
  void controlDisconnected(uint64_t token) { if (token) controlLease_.disconnected(fingerprint(token)); }
  ControlLeaseResult controlResultFor(uint64_t token) const {
    if (!token || controlLease_.lastRequestId() == 0) return ControlLeaseResult::NoPending;
    const uint64_t candidate = fingerprint(token);
    if (candidate == controlLease_.requester() || candidate == controlLease_.lastRequester() ||
        (controlLease_.lastResult() == ControlLeaseResult::Accepted && controlLease_.isOwner(candidate))) {
      return controlLease_.lastResult();
    }
    return ControlLeaseResult::NoPending;
  }
  NoticeboardState current() const { return noticeboard_.current(); }
  uint32_t noticeRevision() const { return noticeRevision_; }
  uint32_t factRevision() const { return factRevision_; }
  const ProposedRaceSetup* proposedRaceSetup() const { return noticeboard_.proposedRaceSetup(); }
  StartReadiness readiness() const { return noticeboard_.readiness(); }
  uint32_t proposalRevision() const { return noticeboard_.proposalRevision(); }
  bool lastFact(Message& fact) const {
    if (!hasFact_) return false;
    fact = lastFact_;
    return true;
  }
  Time scheduledGo() const { return scheduledGo_; }
  bool serverReady() const { return server_ != nullptr; }
  int serverStartError() const { return serverStartError_; }
  bool wifiConnected() const { return wifiConnected_; }
  uint32_t wifiReconnectAttempts() const { return wifiReconnectAttempts_; }
  struct HttpHealth {
    bool wifiConnected = false;
    bool serverReady = false;
    uint32_t requestCount = 0;
    uint32_t requestErrors = 0;
    uint32_t slowRequests = 0;
    uint32_t activeHandlers = 0;
    uint32_t peakHandlers = 0;
    uint32_t serverStarts = 0;
    uint32_t serverStops = 0;
    uint32_t reconnectAttempts = 0;
    uint32_t lastDurationMs = 0;
    int lastError = 0;
    char lastRoute[32]{};
  };
  void httpHealth(HttpHealth& value) const {
    value.wifiConnected = wifiConnected_;
    value.serverReady = server_ != nullptr;
    value.requestCount = httpRequestCount_;
    value.requestErrors = httpRequestErrors_;
    value.slowRequests = httpSlowRequests_;
    value.activeHandlers = httpActiveHandlers_;
    value.peakHandlers = httpPeakHandlers_;
    value.serverStarts = serverStarts_;
    value.serverStops = serverStops_;
    value.reconnectAttempts = wifiReconnectAttempts_;
    value.lastDurationMs = httpLastDurationMs_;
    value.lastError = httpLastError_;
    strncpy(value.lastRoute, httpLastRoute_, sizeof(value.lastRoute) - 1);
  }
  // These bounded serializers are the same production formatting boundary
  // used by the HTTP routes. They are public only so deterministic acceptance
  // fixtures can validate complete payloads without fabricating HTTP state.
  static constexpr size_t StateJsonCapacity = 6144;
  static constexpr size_t ResultsJsonCapacity = 4096;
  static constexpr size_t RecordsJsonCapacity = 2048;
  bool serializeStateForAcceptance(char* out,size_t cap,size_t&length)const{return serializeState(current(),systemTime(),out,cap,length);}
  bool serializeProposalForAcceptance(char*out,size_t cap,size_t&length)const{return serializeProposal(proposedRaceSetup(),controllerEpoch_,proposalRevision(),readiness(),out,cap,length);}
  bool serializePracticeSummaryForAcceptance(char*out,size_t cap,size_t&length)const{return serializePracticeSummary(noticeboard_.practiceSummary(),out,cap,length);}
  bool serializeResultsForAcceptance(char* out,size_t cap,size_t&length)const{auto* loaded=static_cast<RaceEngineModule::CompletedRaceResult*>(malloc(sizeof(RaceEngineModule::CompletedRaceResult)));if(!loaded)return false;const bool ok=serializeResults(displayResult(*loaded),out,cap,length);free(loaded);return ok;}
  bool serializeDetailsForAcceptance(char* out,size_t cap,size_t&length)const{auto* loaded=static_cast<RaceEngineModule::CompletedRaceResult*>(malloc(sizeof(RaceEngineModule::CompletedRaceResult)));if(!loaded)return false;const bool ok=serializeDetails(displayResult(*loaded),out,cap,length);free(loaded);return ok;}
  bool serializeRecordsForAcceptance(char* out,size_t cap,size_t&length)const{return serializeRecords(records_,current().entryCount,out,cap,length);}
  bool serializeHistoryForAcceptance(char* out,size_t cap,size_t&length)const{auto* scratch=static_cast<RaceEngineModule::CompletedRaceResult*>(malloc(sizeof(RaceEngineModule::CompletedRaceResult)));const bool ok=serializeHistory(history_,out,cap,length,scratch);free(scratch);return ok;}
  static bool serializeStateForAcceptance(const NoticeboardState& value,Time now,char*out,size_t cap,size_t&length){return serializeState(value,now,out,cap,length);}
  static bool serializeResultsForAcceptance(const RaceEngineModule::CompletedRaceResult& value,char*out,size_t cap,size_t&length){return serializeResults(value,out,cap,length);}
  static bool serializeDetailsForAcceptance(const RaceEngineModule::CompletedRaceResult& value,char*out,size_t cap,size_t&length){return serializeDetails(value,out,cap,length);}
  static bool serializeRecordsForAcceptance(TrackRecordStore* records,uint8_t count,char*out,size_t cap,size_t&length){return serializeRecords(records,count,out,cap,length);}
private:
  static uint64_t newControllerEpoch() {
    const uint64_t value = (static_cast<uint64_t>(esp_random()) << 32) |
                           static_cast<uint64_t>(esp_random());
    return value ? value : 1;
  }
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
  ControlLease controlLease_{};
  uint64_t controllerEpoch_ = 0;
  diagnostic::ControllerOperationTrace* diagnosticTrace_ = nullptr;
  uint32_t noticeRevision_ = 0;
  // Clock/start-light presentation is a separate P&P-driven stream. It does
  // not advance the Noticeboard revision or generate NOTICEBOARD_CHANGED.
  volatile uint32_t presentationSequence_ = 0;
  volatile uint32_t presentationSentSequence_ = 0;
  bool presentationKnown_ = false;
  NoticeboardPresentationState presentation_{};
  uint32_t factRevision_ = 0;
  uint32_t nextCorrelation_ = 1;
  Message lastFact_{};
  bool hasFact_ = false;
  bool started_ = false;
  bool wifiConnected_ = false;
  uint32_t wifiReconnectAttempts_ = 0;
  uint32_t lastWifiAttemptMs_ = 0;
  uint32_t httpRequestCount_ = 0;
  uint32_t httpRequestErrors_ = 0;
  uint32_t httpSlowRequests_ = 0;
  uint32_t httpActiveHandlers_ = 0;
  uint32_t httpPeakHandlers_ = 0;
  uint32_t httpLastDurationMs_ = 0;
  int httpLastError_ = 0;
  uint32_t httpLastDiagnosticMs_ = 0;
  char httpLastRoute_[32]{};
  uint8_t bootNormalEntries_ = 0;
  uint8_t bootStateEntries_ = 0;
  uint8_t bootProposalEntries_ = 0;
  uint8_t bootContextEntries_ = 0;
  uint8_t bootEventsEntries_ = 0;
  uint8_t bootNormalCompletions_ = 0;
  uint8_t bootStateCompletions_ = 0;
  uint8_t bootProposalCompletions_ = 0;
  uint8_t bootContextCompletions_ = 0;
  uint8_t bootEventsCompletions_ = 0;
  uint32_t serverStarts_ = 0;
  uint32_t serverStops_ = 0;
  struct SseSession {
    BrowserInterface* owner = nullptr;
    httpd_handle_t server = nullptr;
    int fd = -1;
    uint64_t clientToken = 0;
    uint32_t generation = 0;
    volatile bool valid = false;
  };
  struct SseWork {
    BrowserInterface* owner = nullptr;
    uint32_t generation = 0;
  };
  SseSession* sseSession_ = nullptr;
  volatile bool eventConnected_ = false;
  volatile uint32_t eventGeneration_ = 0;
  uint32_t nextEventGeneration_ = 0;
  volatile bool eventWorkQueued_ = false;
  volatile uint32_t eventSentRevision_ = 0;
  uint32_t eventLastSendMs_ = 0;
  volatile bool controlNotificationPending_ = false;
  volatile uint32_t controlNotificationRequestId_ = 0;
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
  bool lapNoticePending_ = false;
  uint32_t lapNoticeRevision_ = 0;
  uint32_t lapNoticeEntry_ = 0;
  uint32_t lapNoticeNumber_ = 0;
  uint32_t lapNoticeEventId_ = 0;
  uint64_t lapNoticeRelevantUs_ = 0;
#endif
  Time scheduledGo_ = 0;
  StoredResult results_[ResultCapacity]{};
  Pending pending_[ResultCapacity]{};
  size_t nextResult_ = 0;
  size_t nextPending_ = 0;
  int serverStartError_ = 0;
  DestructiveConfirmation confirmation_{};
  bool (*fixturePass_)(uint8_t) = nullptr;
  void (*fixtureReset_)() = nullptr;
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
  struct LapDisplayPending {
    bool active = false;
    uint32_t entry = 0;
    uint32_t lap = 0;
    uint32_t eventId = 0;
    uint64_t passUs = 0;
  };
  LapDisplayPending lapDisplayPending_[8]{};
  void recordLapDisplayPass(const Message& message) {
    LapDisplayPending* slot = nullptr;
    for (auto& pending : lapDisplayPending_) {
      if (pending.active && pending.entry == message.raceEntryId && pending.lap == message.lapNumber) {
        slot = &pending;
        break;
      }
      if (!slot && !pending.active) slot = &pending;
    }
    if (!slot) slot = &lapDisplayPending_[message.eventId % 8U];
    slot->active = true;
    slot->entry = message.raceEntryId;
    slot->lap = message.lapNumber;
    slot->eventId = message.eventId;
    slot->passUs = systemTime();
    Serial.printf("[MEASURE] PASS_ACCEPTED lane_entry=%lu lap=%lu event_id=%lu at_us=%llu\n",
      static_cast<unsigned long>(slot->entry), static_cast<unsigned long>(slot->lap),
      static_cast<unsigned long>(slot->eventId), static_cast<unsigned long long>(slot->passUs));
  }
#endif
  struct RequestTrace {
    BrowserInterface* browser;
    const char* route;
    uint32_t started;
    esp_err_t result = ESP_OK;
    RequestTrace(BrowserInterface* value, const char* name)
        : browser(value), route(name), started(millis()) { browser->requestEntered(route); browser->bootRequestEntered(route); }
    ~RequestTrace() { browser->requestCompleted(route, result, static_cast<uint32_t>(millis() - started)); browser->bootRequestCompleted(route, result); }
    void complete(esp_err_t value) { result = value; }
  };
  void bootRequestEntered(const char* route) {
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
    uint8_t* count = nullptr;
    if (strcmp(route, "/normal") == 0) count = &bootNormalEntries_;
    else if (strcmp(route, "/state") == 0) count = &bootStateEntries_;
    else if (strcmp(route, "/proposal") == 0) count = &bootProposalEntries_;
    else if (strcmp(route, "/context") == 0) count = &bootContextEntries_;
    else if (strcmp(route, "/events") == 0) count = &bootEventsEntries_;
    if (count && *count == 0) {
      *count = 1;
      Serial.printf("[BOOTHTTP] enter route=%s at_us=%llu\n", route,
                    static_cast<unsigned long long>(systemTime()));
    }
#else
    (void)route;
#endif
  }
  void bootRequestCompleted(const char* route, esp_err_t result) {
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
    uint8_t* count = nullptr;
    if (strcmp(route, "/normal") == 0) count = &bootNormalCompletions_;
    else if (strcmp(route, "/state") == 0) count = &bootStateCompletions_;
    else if (strcmp(route, "/proposal") == 0) count = &bootProposalCompletions_;
    else if (strcmp(route, "/context") == 0) count = &bootContextCompletions_;
    else if (strcmp(route, "/events") == 0) count = &bootEventsCompletions_;
    if (count && *count == 0) {
      *count = 1;
      Serial.printf("[BOOTHTTP] complete route=%s result=%d at_us=%llu\n", route, int(result),
                    static_cast<unsigned long long>(systemTime()));
    }
#else
    (void)route; (void)result;
#endif
  }
  bool diagnosticAllowed() {
    const uint32_t now = millis();
    if (static_cast<uint32_t>(now - httpLastDiagnosticMs_) < 1000U) return false;
    httpLastDiagnosticMs_ = now;
    return true;
  }
  void requestEntered(const char* route) {
    ++httpRequestCount_;
    ++httpActiveHandlers_;
    if (httpActiveHandlers_ > httpPeakHandlers_) httpPeakHandlers_ = httpActiveHandlers_;
    strncpy(httpLastRoute_, route, sizeof(httpLastRoute_) - 1);
    httpLastRoute_[sizeof(httpLastRoute_) - 1] = 0;
    if (httpActiveHandlers_ > 1 && diagnosticAllowed()) {
      Serial.printf("[DEV] HTTP overlap route=%s active=%lu heap=%lu\n", route,
                    static_cast<unsigned long>(httpActiveHandlers_),
                    static_cast<unsigned long>(ESP.getFreeHeap()));
    }
  }
  void requestCompleted(const char* route, esp_err_t result, uint32_t durationMs) {
    if (httpActiveHandlers_) --httpActiveHandlers_;
    httpLastDurationMs_ = durationMs;
    httpLastError_ = result == ESP_OK ? 0 : int(result);
    if (result != ESP_OK) ++httpRequestErrors_;
    if (durationMs >= 500) ++httpSlowRequests_;
    if ((result != ESP_OK || durationMs >= 500) && diagnosticAllowed()) {
      Serial.printf("[DEV] HTTP %s route=%s duration_ms=%lu result=%d active=%lu heap=%lu min_heap=%lu\n",
                    result == ESP_OK ? "SLOW" : "ERROR", route,
                    static_cast<unsigned long>(durationMs), int(result),
                    static_cast<unsigned long>(httpActiveHandlers_),
                    static_cast<unsigned long>(ESP.getFreeHeap()),
                    static_cast<unsigned long>(ESP.getMinFreeHeap()));
    }
  }
  const RaceEngineModule::CompletedRaceResult& displayResult(RaceEngineModule::CompletedRaceResult& loaded) const {
    bool found=false; return displayResult(loaded,0,found);
  }
  const RaceEngineModule::CompletedRaceResult& displayResult(RaceEngineModule::CompletedRaceResult& loaded,uint32_t sequence,bool& found) const {
    found=false;
    const auto& current=noticeboard_.completedResult();
    if(sequence){
      size_t bytes=0; uint32_t stored=0;
      if(history_&&history_->loadSequence(sequence,reinterpret_cast<uint8_t*>(&loaded),sizeof(loaded),bytes,stored)&&bytes==sizeof(loaded)&&stored==sequence&&loaded.sealed&&loaded.formatVersion==RaceEngineModule::ResultFormatVersion){found=true;return loaded;}
      RaceEngineModule::clearResult(loaded); return loaded;
    }
    // A sealed engine result is not a Results/Race Again boundary until Race
    // Control has published FINISHED. Do not expose it while completion is
    // still being settled.
    if(current.sealed){
      if(noticeboard_.current().lifecycle!=SessionLifecycle::Finished){RaceEngineModule::clearResult(loaded);return loaded;}
      return current;
    }
    if(!history_) return current;
    size_t bytes=0;uint32_t sequenceValue=0;
    if(history_->loadNewest(0,reinterpret_cast<uint8_t*>(&loaded),sizeof(loaded),bytes,sequenceValue)&&bytes==sizeof(loaded)&&loaded.sealed&&loaded.formatVersion==RaceEngineModule::ResultFormatVersion)return loaded;
    return current;
  }
  static bool historySequenceQuery(httpd_req_t* request,uint32_t& sequence){
    sequence=0; char query[48]{}, raw[16]{};
    if(httpd_req_get_url_query_str(request,query,sizeof(query))!=ESP_OK)return true;
    if(httpd_query_key_value(query,"sequence",raw,sizeof(raw))!=ESP_OK)return false;
    char* end=nullptr; const unsigned long value=strtoul(raw,&end,10);
    if(!end||*end||value==0||value>UINT32_MAX)return false; sequence=uint32_t(value); return true;
  }
  static uint64_t fingerprint(uint64_t token) {
    token ^= token >> 33;
    token *= 0xff51afd7ed558ccdULL;
    token ^= token >> 33;
    return token ? token : 1;
  }
  static diagnostic::OperationName diagnosticOperation(SessionOperation operation) {
    switch (operation) {
      case SessionOperation::Pause: return diagnostic::OperationName::Pause;
      case SessionOperation::Resume: return diagnostic::OperationName::Resume;
      case SessionOperation::RestartRace: return diagnostic::OperationName::RestartRace;
      case SessionOperation::EndRace: return diagnostic::OperationName::EndRace;
      case SessionOperation::RaceAgain: return diagnostic::OperationName::RaceAgain;
      case SessionOperation::EndSession: return diagnostic::OperationName::EndSession;
      case SessionOperation::SkipFinishDisplay: return diagnostic::OperationName::SkipFinishDisplay;
      default: return diagnostic::OperationName::Unknown;
    }
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
    size_t n=0;const char* mode=value.sessionMode==SessionMode::OpenPractice?"OPEN_PRACTICE":value.sessionMode==SessionMode::Endurance?"ENDURANCE":value.sessionMode==SessionMode::None?"NONE":"LAP_RACE";bool ok=appendJson(out,cap,n,"{\"systemTime\":%llu,\"lifecycle\":\"%s\",\"sessionMode\":\"%s\",\"proposalRevision\":%lu,\"entryCount\":%u,\"raceEntryId\":%lu,\"laps\":%lu,\"lapTarget\":%lu,\"durationMinutes\":%u,\"durationExpiryAt\":%llu,\"remainingDuration\":%llu,\"durationExpired\":%s,\"overtime\":%llu,\"finishBehaviour\":\"%s\",\"hasLap\":%s,\"lastLapTime\":%llu,\"sessionFastestLap\":%llu,\"scheduledGo\":%llu,\"redLightCount\":%u,\"redIntervalUs\":%llu,\"finalDelayUs\":%llu,\"startSignal\":%u,\"pauseEffectiveAt\":%llu,\"scheduledRestartAt\":%llu,\"restartMethod\":\"%s\",\"resultValid\":%s,\"raceIntegrity\":\"%s\",\"resultSealed\":%s,\"winningTime\":%llu,\"finishTime\":%llu,\"fastestLap\":%llu,\"historySequence\":%lu,\"persistencePending\":%s,\"persistenceFault\":%s,\"entries\":[",
      (unsigned long long)now,lifecycle(value.lifecycle),mode,(unsigned long)value.proposalRevision,unsigned(value.entryCount),(unsigned long)value.raceEntryId,(unsigned long)value.laps,(unsigned long)value.lapTarget,unsigned(value.durationMinutes),(unsigned long long)value.durationExpiryAt,(unsigned long long)value.remainingDuration,value.durationExpired?"true":"false",(unsigned long long)value.overtime,finishBehaviour(value.finishBehaviour),value.hasLap?"true":"false",(unsigned long long)value.lastLapTime,(unsigned long long)value.sessionFastestLap,(unsigned long long)value.scheduledGo,unsigned(value.redLightCount),(unsigned long long)value.redIntervalUs,(unsigned long long)value.finalDelayUs,unsigned(value.startSignal),(unsigned long long)value.pauseEffectiveAt,(unsigned long long)value.scheduledRestartAt,value.restartMethod==RestartMethod::Honour?"HONOUR":value.restartMethod==RestartMethod::Grid?"GRID":"NONE",value.resultValid?"true":"false",value.raceIntegrityFaulted?"FAULTED":"OK",value.resultSealed?"true":"false",(unsigned long long)value.winningTime,(unsigned long long)value.finishTime,(unsigned long long)value.fastestLap,(unsigned long)value.historySequence,value.persistencePending?"true":"false",value.persistenceFault?"true":"false");
    if(value.entryCount>PP_MAX_ENTRIES)ok=false;const uint8_t count=value.entryCount>PP_MAX_ENTRIES?PP_MAX_ENTRIES:value.entryCount;
    for(uint8_t i=0;i<count;++i){const auto&e=value.entries[i];const char* gap=e.gapKind==RaceEngineModule::LiveGapKind::Laps?"LAPS":e.gapKind==RaceEngineModule::LiveGapKind::Time?"TIME":"NONE";ok=appendJson(out,cap,n,"%s{\"raceEntryId\":%lu,\"mugId\":%lu,\"lane\":%u,\"position\":%lu,\"lapsBehind\":%lu,\"gapKind\":\"%s\",\"gapTimeUs\":%llu,\"gapLaps\":%lu,\"laps\":%lu,\"classifiedLaps\":%lu,\"lapPenalty\":%lu,\"lastLapTime\":%llu,\"bestLapTime\":%llu,\"hasLap\":%s,\"waitingForTimingOrigin\":%s}",i?",":"",(unsigned long)e.raceEntryId,(unsigned long)e.mugId,unsigned(e.lane),(unsigned long)e.position,(unsigned long)e.lapsBehind,gap,(unsigned long long)e.gapTime,(unsigned long)e.lapsBehind,(unsigned long)e.laps,(unsigned long)e.classifiedLaps,(unsigned long)e.lapPenalty,(unsigned long long)e.lastLapTime,(unsigned long long)e.bestLapTime,e.hasLap?"true":"false",e.waitingForTimingOrigin?"true":"false")&&ok;}
    ok=appendJson(out,cap,n,"],\"startPresentation\":{\"phase\":\"%s\",\"goAt\":%llu,\"redLightsLit\":%u,\"redLightCount\":%u,\"green\":%s,\"active\":%s},\"finishDisplay\":{\"active\":%s,\"until\":%llu,\"remainingUs\":%llu,\"durationSeconds\":%u},\"practiceSummaryAvailable\":%s}",startPhase(value.startPhase),(unsigned long long)value.startPhaseUntil,unsigned(value.redLightsLit),unsigned(value.redLightCount),value.startLightsGreen?"true":"false",value.startPhase!=StartPresentationPhase::None?"true":"false",value.finishDisplayActive?"true":"false",(unsigned long long)value.finishDisplayUntil,(unsigned long long)value.finishDisplayRemaining,unsigned(value.finishDisplayDurationSeconds),value.practiceSummaryAvailable?"true":"false")&&ok;length=n;return ok;
  }
  static const char* setupMode(SessionMode mode){return mode==SessionMode::OpenPractice?"OPEN_PRACTICE":mode==SessionMode::Endurance?"ENDURANCE":mode==SessionMode::None?"NONE":"LAP_RACE";}
  static const char* readinessText(StartReadiness value){switch(value){case StartReadiness::Ready:return "READY";case StartReadiness::InvalidRaceSetup:return "INVALID_RACE_SETUP";case StartReadiness::CapabilityUnavailable:return "CAPABILITY_UNAVAILABLE";default:return "LIFECYCLE_NOT_STARTABLE";}}
  static bool serializeProposal(const ProposedRaceSetup* setup,uint64_t epoch,uint32_t revision,StartReadiness readiness,char*out,size_t cap,size_t&length){
    if(!setup)return false;size_t n=0;const bool blocking=readiness!=StartReadiness::Ready;const char*severity=blocking?"BLOCKING":"NONE";bool ok=appendJson(out,cap,n,"{\"formatVersion\":1,\"controllerEpoch\":\"%llu\",\"proposalRevision\":%lu,\"startable\":%s,\"readiness\":\"%s\",\"readinessContext\":{\"version\":1,\"code\":\"%s\",\"severity\":\"%s\",\"blocking\":%s,\"lane\":0},\"mode\":\"%s\",\"lapTarget\":%lu,\"durationMinutes\":%u,\"finishBehaviour\":\"%s\",\"activeLanes\":%u,\"supportedModes\":[{\"mode\":\"LAP_RACE\",\"available\":true},{\"mode\":\"OPEN_PRACTICE\",\"available\":true},{\"mode\":\"ENDURANCE\",\"available\":true},{\"mode\":\"TIMED_STAGE\",\"available\":false},{\"mode\":\"DRAG\",\"available\":false}],\"entries\":[",(unsigned long long)epoch,(unsigned long)revision,readiness==StartReadiness::Ready?"true":"false",readinessText(readiness),readinessCode(readiness),severity,blocking?"true":"false",setupMode(setup->mode),(unsigned long)setup->lapTarget,unsigned(setup->durationMinutes),finishBehaviour(setup->finish),unsigned(setup->activeLanes));
    for(uint8_t i=0;i<setup->activeLanes&&i<PP_MAX_ENTRIES;++i){const auto&e=setup->entries[i];ok=appendJson(out,cap,n,"%s{\"index\":%u,\"lane\":%u,\"inputDevice\":%lu,\"inputCapability\":%u,\"mugId\":%lu}",i?",":"",unsigned(i),unsigned(e.startFinish.lane),(unsigned long)e.startFinish.input.device,unsigned(e.startFinish.input.capability),(unsigned long)e.mugId)&&ok;}
    ok=appendJson(out,cap,n,"]}")&&ok;length=n;return ok;
  }
  static bool serializeResults(const RaceEngineModule::CompletedRaceResult&r,char*out,size_t cap,size_t&length){
    const uint64_t overtimeUs=r.overtime&&r.finishTime>r.expiryTime?r.finishTime-r.expiryTime:0;size_t n=0;const char* mode=r.mode==SessionMode::Endurance?"ENDURANCE":r.mode==SessionMode::OpenPractice?"OPEN_PRACTICE":"LAP_RACE";bool ok=true;ok=appendJson(out,cap,n,"{\"sealed\":%s,\"valid\":%s,\"mode\":\"%s\"",r.sealed?"true":"false",r.valid?"true":"false",mode)&&ok;ok=appendJson(out,cap,n,",\"lapTarget\":%lu,\"durationMinutes\":%u,\"durationUs\":%llu",(unsigned long)r.lapTarget,unsigned(r.durationMinutes),(unsigned long long)r.durationUs)&&ok;ok=appendJson(out,cap,n,",\"expiryTime\":%llu,\"overtime\":%s,\"overtimeUs\":%llu",(unsigned long long)r.expiryTime,r.overtime?"true":"false",(unsigned long long)overtimeUs)&&ok;ok=appendJson(out,cap,n,",\"winningTime\":%llu,\"finishTime\":%llu,\"finishBehaviour\":\"%s\"",(unsigned long long)r.winningTime,(unsigned long long)r.finishTime,finishBehaviour(r.behaviour))&&ok;ok=appendJson(out,cap,n,",\"deadHeat\":%s,\"fastestLap\":%llu,\"fastestEntryId\":%lu,\"fastestLapTied\":%s,\"entries\":[",r.deadHeat?"true":"false",(unsigned long long)r.fastestLap,(unsigned long)r.fastestEntryId,r.fastestLapTied?"true":"false")&&ok;
    if(r.entryCount>PP_MAX_ENTRIES)ok=false;const uint8_t count=r.entryCount>PP_MAX_ENTRIES?PP_MAX_ENTRIES:r.entryCount;
    for(uint8_t i=0;i<count;++i){const auto&e=r.entries[i];ok=appendJson(out,cap,n,"%s{\"raceEntryId\":%lu,\"mugId\":%lu,\"lane\":%u,\"laps\":%lu,\"classifiedLaps\":%lu,\"lapPenalty\":%lu,\"rank\":%lu,\"tied\":%s,\"lapsBehind\":%lu,\"completed\":%s,\"completionTime\":%llu,\"bestLap\":%llu}",i?",":"",(unsigned long)e.raceEntryId,(unsigned long)e.mugId,unsigned(e.lane),(unsigned long)e.laps,(unsigned long)e.classifiedLaps,(unsigned long)e.lapPenalty,(unsigned long)e.rank,e.tied?"true":"false",(unsigned long)e.lapsBehind,e.completed?"true":"false",(unsigned long long)e.completionTime,(unsigned long long)e.bestLap)&&ok;}
    ok=appendJson(out,cap,n,"]}")&&ok;length=n;return ok;
  }
  static bool serializeDetails(const RaceEngineModule::CompletedRaceResult&r,char*out,size_t cap,size_t&length){
    size_t n=0;bool ok=appendJson(out,cap,n,"{\"sealed\":%s,\"lapTarget\":%lu,\"entries\":[",r.sealed?"true":"false",(unsigned long)r.lapTarget);
    if(r.entryCount>PP_MAX_ENTRIES)ok=false;const uint8_t count=r.entryCount>PP_MAX_ENTRIES?PP_MAX_ENTRIES:r.entryCount;
    for(uint8_t i=0;i<count;++i){const auto&e=r.entries[i];ok=appendJson(out,cap,n,"%s{\"raceEntryId\":%lu,\"mugId\":%lu,\"lane\":%u,\"laps\":%lu,\"rank\":%lu,\"records\":[",i?",":"",(unsigned long)e.raceEntryId,(unsigned long)e.mugId,unsigned(e.lane),(unsigned long)e.laps,(unsigned long)e.rank)&&ok;for(uint8_t j=0;j<e.recordCount&&j<RaceEngineModule::MaxLaps;++j){const auto&lap=e.records[j];ok=appendJson(out,cap,n,"%s{\"lapNumber\":%lu,\"startTime\":%llu,\"finishTime\":%llu,\"lapTime\":%llu,\"valid\":%s}",j?",":"",(unsigned long)lap.lapNumber,(unsigned long long)lap.startTime,(unsigned long long)lap.finishTime,(unsigned long long)lap.lapTime,lap.valid?"true":"false")&&ok;}ok=appendJson(out,cap,n,"]}")&&ok;}
    ok=appendJson(out,cap,n,"]}")&&ok;length=n;return ok;
  }
  static bool serializeRecords(TrackRecordStore* records,uint8_t count,char*out,size_t cap,size_t&length){
    size_t n=0;const uint32_t era=records?records->era():0;const Time lane1=records?records->lanePb(1):0;const Time lane2=records?records->lanePb(2):0;const Time track=records?records->trackRecord():0;bool ok=appendJson(out,cap,n,"{\"track\":\"DEFAULT TRACK\",\"era\":%lu,\"lane1Pb\":%llu,\"lane2Pb\":%llu,\"trackRecord\":%llu,\"entries\":[",(unsigned long)era,(unsigned long long)lane1,(unsigned long long)lane2,(unsigned long long)track);if(count>PP_MAX_ENTRIES)ok=false;const uint8_t bounded=count>PP_MAX_ENTRIES?PP_MAX_ENTRIES:count;for(uint8_t i=1;i<=bounded;++i){const Time pb=records?records->lanePb(i):0;ok=appendJson(out,cap,n,"%s{\"lane\":%u,\"pb\":%llu}",i==1?"":",",unsigned(i),(unsigned long long)pb)&&ok;}ok=appendJson(out,cap,n,"]}")&&ok;length=n;return ok;
  }
  static bool serializePracticeSummary(const RaceEngineModule::PracticeSummary& value,char*out,size_t cap,size_t&length){
    size_t n=0;bool ok=appendJson(out,cap,n,"{\"formatVersion\":%u,\"available\":%s,\"sessionId\":%lu,\"endedAt\":%llu,\"sessionFastestLap\":%llu,\"sessionFastestEntryId\":%lu,\"entries\":[",unsigned(value.formatVersion),value.available?"true":"false",(unsigned long)value.sessionId,(unsigned long long)value.endedAt,(unsigned long long)value.sessionFastestLap,(unsigned long)value.sessionFastestEntryId);
    const uint8_t count=value.entryCount>PP_MAX_ENTRIES?PP_MAX_ENTRIES:value.entryCount;if(value.entryCount>PP_MAX_ENTRIES)ok=false;
    for(uint8_t i=0;i<count&&ok;++i){const auto&e=value.entries[i];ok=appendJson(out,cap,n,"%s{\"raceEntryId\":%lu,\"mugId\":%lu,\"lane\":%u,\"laps\":%lu,\"lastLap\":%llu,\"bestLap\":%llu}",i?",":"",(unsigned long)e.raceEntryId,(unsigned long)e.mugId,unsigned(e.lane),(unsigned long)e.laps,(unsigned long long)e.lastLap,(unsigned long long)e.bestLap)&&ok;}
    ok=appendJson(out,cap,n,"]}")&&ok;length=n;return ok;
  }
  static bool serializeHistory(HistoryStore* history,char*out,size_t cap,size_t&length,RaceEngineModule::CompletedRaceResult* scratch=nullptr){
    size_t n=0;bool ok=appendJson(out,cap,n,"{\"entries\":[");
    auto* loaded=scratch?scratch:static_cast<RaceEngineModule::CompletedRaceResult*>(malloc(sizeof(RaceEngineModule::CompletedRaceResult)));const bool owned=!scratch;
    if(!loaded){length=0;return false;}
    if(history){
      const uint8_t total=history->count();
      for(uint8_t i=0;i<total&&ok;++i){
        size_t bytes=0;uint32_t sequence=0;
        if(!history->loadNewest(i,reinterpret_cast<uint8_t*>(loaded),sizeof(*loaded),bytes,sequence)||bytes!=sizeof(*loaded)||!loaded->sealed||loaded->formatVersion!=RaceEngineModule::ResultFormatVersion){ok=false;break;}
        const char* mode=loaded->mode==SessionMode::Endurance?"ENDURANCE":loaded->mode==SessionMode::OpenPractice?"OPEN_PRACTICE":"LAP_RACE";
        const uint64_t overtimeUs=loaded->overtime&&loaded->finishTime>loaded->expiryTime?loaded->finishTime-loaded->expiryTime:0;ok=appendJson(out,cap,n,"%s{\"sequence\":%lu,\"sealed\":true,\"mode\":\"%s\",\"durationMinutes\":%u,\"lapTarget\":%lu,\"finishBehaviour\":\"%s\",\"winningTime\":%llu,\"finishTime\":%llu,\"expiryTime\":%llu,\"overtime\":%s,\"overtimeUs\":%llu,\"fastestLap\":%llu,\"entryCount\":%u,\"entries\":[",i?",":"",(unsigned long)sequence,mode,unsigned(loaded->durationMinutes),(unsigned long)loaded->lapTarget,finishBehaviour(loaded->behaviour),(unsigned long long)loaded->winningTime,(unsigned long long)loaded->finishTime,(unsigned long long)loaded->expiryTime,loaded->overtime?"true":"false",(unsigned long long)overtimeUs,(unsigned long long)loaded->fastestLap,unsigned(loaded->entryCount));
        const uint8_t count=loaded->entryCount>PP_MAX_ENTRIES?PP_MAX_ENTRIES:loaded->entryCount;if(loaded->entryCount>PP_MAX_ENTRIES)ok=false;
        for(uint8_t j=0;j<count&&ok;++j){const auto&e=loaded->entries[j];ok=appendJson(out,cap,n,"%s{\"raceEntryId\":%lu,\"mugId\":%lu,\"lane\":%u,\"laps\":%lu,\"classifiedLaps\":%lu,\"lapPenalty\":%lu,\"rank\":%lu,\"lapsBehind\":%lu,\"completed\":%s}",j?",":"",(unsigned long)e.raceEntryId,(unsigned long)e.mugId,unsigned(e.lane),(unsigned long)e.laps,(unsigned long)e.classifiedLaps,(unsigned long)e.lapPenalty,(unsigned long)e.rank,(unsigned long)e.lapsBehind,e.completed?"true":"false");}
        ok=appendJson(out,cap,n,"]}");
      }
    }
    ok=appendJson(out,cap,n,"]}")&&ok;length=n;if(owned)free(loaded);return ok;
  }
  static esp_err_t state(httpd_req_t* request) {
    RequestTrace trace(instance(), "/state");
    client(request);
    const uint64_t traceEnterUs = systemTime();
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
    const int traceFd = httpd_req_to_sockfd(request);
    Serial.printf("[HTTP STATE] enter fd=%d at_us=%llu\\n", traceFd,
                  static_cast<unsigned long long>(traceEnterUs));
#endif
    const uint64_t snapshotStartUs = systemTime();
    const NoticeboardState value = instance()->current();
    const uint64_t snapshotEndUs = systemTime();
    const uint64_t serializeStartUs = systemTime();
    char* json=static_cast<char*>(malloc(StateJsonCapacity)); size_t length=0;
    if(!json){httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"state buffer unavailable");trace.complete(ESP_ERR_NO_MEM);return ESP_FAIL;}
    if(!serializeState(value,systemTime(),json,StateJsonCapacity,length)){free(json);httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"state entry count exceeds capacity");trace.complete(ESP_ERR_INVALID_SIZE);return ESP_FAIL;}
    const uint64_t serializeEndUs = systemTime();
    httpd_resp_set_type(request,"application/json"); httpd_resp_set_hdr(request,"Cache-Control","no-store");
    const uint64_t sendStartUs = systemTime();
    const esp_err_t sent=httpd_resp_sendstr(request,json);
    const uint64_t sendEndUs = systemTime();
    free(json);trace.complete(sent);
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
    const uint64_t exitUs = systemTime();
    Serial.printf("[HTTP STATE] exit fd=%d result=%d enter=%llu snapshot=%llu-%llu serialize=%llu-%llu send=%llu-%llu exit=%llu bytes=%u\\n",
                  traceFd, int(sent), static_cast<unsigned long long>(traceEnterUs),
                  static_cast<unsigned long long>(snapshotStartUs), static_cast<unsigned long long>(snapshotEndUs),
                  static_cast<unsigned long long>(serializeStartUs), static_cast<unsigned long long>(serializeEndUs),
                  static_cast<unsigned long long>(sendStartUs), static_cast<unsigned long long>(sendEndUs),
                  static_cast<unsigned long long>(exitUs), static_cast<unsigned>(length));
#endif
    return sent;
  }
  static const char* finishBehaviour(LapFinishBehaviour value) {
    return value==LapFinishBehaviour::CompleteCurrentLap?"COMPLETE_CURRENT_LAP":value==LapFinishBehaviour::CompleteFullRaceDistance?"COMPLETE_FULL_RACE_DISTANCE":"IMMEDIATE";
  }
  static const char* startPhase(StartPresentationPhase value) {
    return value==StartPresentationPhase::Countdown?"COUNTDOWN":value==StartPresentationPhase::ResumeCountdown?"RESUME_COUNTDOWN":value==StartPresentationPhase::Go?"GO":"NONE";
  }
  static const char* readinessCode(StartReadiness value) {
    return value==StartReadiness::Ready?"READY":value==StartReadiness::InvalidRaceSetup?"INVALID_RACE_SETUP":value==StartReadiness::CapabilityUnavailable?"CAPABILITY_UNAVAILABLE":"LIFECYCLE_NOT_STARTABLE";
  }
  static esp_err_t resultsRoute(httpd_req_t* request) {
    RequestTrace trace(instance(), "/results"); client(request); uint32_t sequence=0;
    if(!historySequenceQuery(request,sequence)){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"invalid history sequence");trace.complete(ESP_ERR_INVALID_ARG);return ESP_FAIL;}
    auto* loaded=static_cast<RaceEngineModule::CompletedRaceResult*>(malloc(sizeof(RaceEngineModule::CompletedRaceResult))); char* json=static_cast<char*>(malloc(ResultsJsonCapacity));
    if(!loaded||!json){free(loaded);free(json);httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"result buffer unavailable");trace.complete(ESP_ERR_NO_MEM);return ESP_FAIL;}
    bool found=false; const auto&r=instance()->displayResult(*loaded,sequence,found); if(sequence&&!found){free(loaded);free(json);httpd_resp_send_err(request,HTTPD_404_NOT_FOUND,"history result not found");trace.complete(ESP_ERR_NOT_FOUND);return ESP_FAIL;}
    size_t length=0; if(!serializeResults(r,json,ResultsJsonCapacity,length)){free(loaded);free(json);httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"result entry count exceeds capacity");trace.complete(ESP_ERR_INVALID_SIZE);return ESP_FAIL;}
    httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");const esp_err_t sent=httpd_resp_sendstr(request,json);free(loaded);free(json);trace.complete(sent);return sent;
  }
  static esp_err_t detailsRoute(httpd_req_t* request) {
    RequestTrace trace(instance(), "/details"); client(request); uint32_t sequence=0;
    if(!historySequenceQuery(request,sequence)){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"invalid history sequence");trace.complete(ESP_ERR_INVALID_ARG);return ESP_FAIL;}
    auto* loaded=static_cast<RaceEngineModule::CompletedRaceResult*>(malloc(sizeof(RaceEngineModule::CompletedRaceResult))); char* chunk=static_cast<char*>(malloc(320));
    if(!loaded||!chunk){free(loaded);free(chunk);httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"details buffer unavailable");trace.complete(ESP_ERR_NO_MEM);return ESP_FAIL;}
    bool found=false; const auto&r=instance()->displayResult(*loaded,sequence,found); if(sequence&&!found){free(loaded);free(chunk);httpd_resp_send_err(request,HTTPD_404_NOT_FOUND,"history result not found");trace.complete(ESP_ERR_NOT_FOUND);return ESP_FAIL;}
    httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");snprintf(chunk,320,"{\"sealed\":%s,\"lapTarget\":%lu,\"entries\":[",r.sealed?"true":"false",(unsigned long)r.lapTarget);httpd_resp_send_chunk(request,chunk,HTTPD_RESP_USE_STRLEN);
    for(uint8_t i=0;i<r.entryCount;++i){const auto&e=r.entries[i];snprintf(chunk,320,"%s{\"raceEntryId\":%lu,\"mugId\":%lu,\"lane\":%u,\"laps\":%lu,\"rank\":%lu,\"records\":[",i?",":"",(unsigned long)e.raceEntryId,(unsigned long)e.mugId,unsigned(e.lane),(unsigned long)e.laps,(unsigned long)e.rank);httpd_resp_send_chunk(request,chunk,HTTPD_RESP_USE_STRLEN);for(uint8_t j=0;j<e.recordCount;++j){const auto&lap=e.records[j];snprintf(chunk,320,"%s{\"lapNumber\":%lu,\"startTime\":%llu,\"finishTime\":%llu,\"lapTime\":%llu,\"valid\":%s}",j?",":"",(unsigned long)lap.lapNumber,(unsigned long long)lap.startTime,(unsigned long long)lap.finishTime,(unsigned long long)lap.lapTime,lap.valid?"true":"false");httpd_resp_send_chunk(request,chunk,HTTPD_RESP_USE_STRLEN);}httpd_resp_send_chunk(request,"]}",2);}
    httpd_resp_send_chunk(request,"]}",2);const esp_err_t sent=httpd_resp_send_chunk(request,nullptr,0);free(loaded);free(chunk);trace.complete(sent);return sent;
  }
  static esp_err_t historyRoute(httpd_req_t* request) {
    RequestTrace trace(instance(), "/history"); client(request); BrowserInterface* browser=instance();
    constexpr size_t HistoryJsonCapacity=4096; char* json=static_cast<char*>(malloc(HistoryJsonCapacity)); size_t length=0;
    auto* stored=static_cast<RaceEngineModule::CompletedRaceResult*>(malloc(sizeof(RaceEngineModule::CompletedRaceResult)));
    if(!json||!stored){free(stored);free(json);httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"history buffer unavailable");trace.complete(ESP_ERR_NO_MEM);return ESP_FAIL;}
    if(!serializeHistory(browser->history_,json,HistoryJsonCapacity,length,stored)){free(stored);free(json);httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"history exceeds response capacity or contains an incompatible result");trace.complete(ESP_ERR_INVALID_SIZE);return ESP_FAIL;}
    httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");const esp_err_t sent=httpd_resp_sendstr(request,json);free(stored);free(json);trace.complete(sent);return sent;
  }
  static esp_err_t recordsRoute(httpd_req_t* request) {
    client(request); BrowserInterface* browser=instance();
    char* json=static_cast<char*>(malloc(RecordsJsonCapacity));size_t length=0;
    if(!json){httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"records buffer unavailable");return ESP_FAIL;}
    if(!serializeRecords(browser->records_,browser->noticeboard_.current().entryCount,json,RecordsJsonCapacity,length)){free(json);httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"record entry count exceeds capacity");return ESP_FAIL;}
    httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");const esp_err_t sent=httpd_resp_sendstr(request,json);free(json);return sent;
  }
  static esp_err_t notice(httpd_req_t* request) {
    RequestTrace trace(instance(), "/noticeboard");
    client(request);
    char json[128];
    snprintf(json, sizeof(json), "{\"type\":\"NOTICEBOARD_CHANGED\",\"revision\":%lu,\"proposalRevision\":%lu}",
             (unsigned long)instance()->noticeRevision(),(unsigned long)instance()->proposalRevision());
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    const esp_err_t sent = httpd_resp_sendstr(request, json); trace.complete(sent); return sent;
  }
  static void freeSseSession(void* context) {
    auto* session = static_cast<SseSession*>(context);
    if (!session) return;
    session->valid = false;
    BrowserInterface* browser = session->owner;
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
    Serial.printf("[SSE] disconnected generation=%lu fd=%d at_us=%llu\n",
      static_cast<unsigned long>(session->generation), session->fd,
      static_cast<unsigned long long>(systemTime()));
#endif
    if (browser && browser->sseSession_ == session) {
      browser->sseSession_ = nullptr;
      browser->eventConnected_ = false;
      browser->eventGeneration_ = 0;
      browser->controlDisconnected(session->clientToken);
    }
    delete session;
  }
  static void eventWork(void* argument) {
    auto* work = static_cast<SseWork*>(argument);
    if (!work) return;
    if (work->owner) work->owner->sendEventNotification(work->generation);
    delete work;
  }
  void updatePresentation(Time now) {
    const NoticeboardPresentationState next = noticeboard_.presentationAt(now);
    const uint64_t nextSecond = next.durationExpired
      ? next.overtime / 1000000ULL
      : next.remainingDuration / 1000000ULL;
    const uint64_t oldSecond = presentation_.durationExpired
      ? presentation_.overtime / 1000000ULL
      : presentation_.remainingDuration / 1000000ULL;
    const bool changed = !presentationKnown_ ||
      next.lifecycle != presentation_.lifecycle ||
      next.sessionMode != presentation_.sessionMode ||
      next.durationExpired != presentation_.durationExpired ||
      nextSecond != oldSecond ||
      next.startPhase != presentation_.startPhase ||
      next.redLightsLit != presentation_.redLightsLit ||
      next.redLightCount != presentation_.redLightCount ||
      next.startLightsGreen != presentation_.startLightsGreen;
    if (changed) {
      presentation_ = next;
      presentationKnown_ = true;
      ++presentationSequence_;
    }
  }

  void queueEventWork() {
    if (!server_ || !eventConnected_ || eventGeneration_ == 0 || eventWorkQueued_) return;
    auto* work = new SseWork{};
    if (!work) return;
    work->owner = this;
    work->generation = eventGeneration_;
    eventWorkQueued_ = true;
    const esp_err_t queued = httpd_queue_work(server_, &BrowserInterface::eventWork, work);
    if (queued != ESP_OK) {
      eventWorkQueued_ = false;
      delete work;
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
      Serial.printf("[SSE] queue_failed revision=%lu at_us=%llu result=%d\n",
        static_cast<unsigned long>(noticeRevision_),
        static_cast<unsigned long long>(systemTime()), int(queued));
#endif
      return;
    }
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
    const bool lap = lapNoticeRevision_ == noticeRevision_;
    Serial.printf("[SSE] queued generation=%lu revision=%lu lap_entry=%lu lap=%lu event_id=%lu notify_us=%llu\n",
      static_cast<unsigned long>(eventGeneration_),
      static_cast<unsigned long>(noticeRevision_),
      static_cast<unsigned long>(lap ? lapNoticeEntry_ : 0),
      static_cast<unsigned long>(lap ? lapNoticeNumber_ : 0),
      static_cast<unsigned long>(lap ? lapNoticeEventId_ : 0),
      static_cast<unsigned long long>(systemTime()));
#endif
  }
  void sendEventNotification(uint32_t generation) {
    const httpd_handle_t handle = server_;
    SseSession* session = sseSession_;
    if (!handle || !session || !session->valid || session->server != handle ||
        session->generation != generation || eventGeneration_ != generation) {
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
      Serial.printf("[SSE] stale_work generation=%lu current=%lu at_us=%llu\n",
        static_cast<unsigned long>(generation),
        static_cast<unsigned long>(eventGeneration_),
        static_cast<unsigned long long>(systemTime()));
#endif
      eventWorkQueued_ = false;
      return;
    }
    const int fd = session->fd;
    if (httpd_sess_get_ctx(handle, fd) != session) {
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
      Serial.printf("[SSE] session_mismatch generation=%lu fd=%d at_us=%llu\n",
        static_cast<unsigned long>(generation), fd,
        static_cast<unsigned long long>(systemTime()));
#endif
      session->valid = false;
      if (sseSession_ == session) {
        sseSession_ = nullptr;
        eventConnected_ = false;
        eventGeneration_ = 0;
      }
      eventWorkQueued_ = false;
      return;
    }
    char payload[192];
    const uint32_t revision = noticeRevision_;
    const bool noticePending = revision != eventSentRevision_;
    const uint32_t presentationSequence = presentationSequence_;
    const bool presentationPending = presentationSequence != presentationSentSequence_;
    const bool controlPending = controlNotificationPending_;
    const uint32_t controlRequestId = controlNotificationRequestId_;
    const bool keepalive = !noticePending && !presentationPending && !controlPending;
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
    const uint32_t payloadEntry = lapNoticeRevision_ == revision ? lapNoticeEntry_ : 0;
    const uint32_t payloadLap = lapNoticeRevision_ == revision ? lapNoticeNumber_ : 0;
    const uint32_t payloadEventId = lapNoticeRevision_ == revision ? lapNoticeEventId_ : 0;
#else
    const uint32_t payloadEntry = 0;
    const uint32_t payloadLap = 0;
    const uint32_t payloadEventId = 0;
#endif
    int length = 0;
    if (keepalive) {
      length = snprintf(payload, sizeof(payload), ": ping\n\n");
    } else if (noticePending) {
      length = snprintf(payload, sizeof(payload), "data: {\"revision\":%lu,\"serverUs\":%llu,\"lapEntry\":%lu,\"lap\":%lu,\"eventId\":%lu}\n\n",
                        (unsigned long)revision, (unsigned long long)systemTime(),
                        static_cast<unsigned long>(payloadEntry), static_cast<unsigned long>(payloadLap),
                        static_cast<unsigned long>(payloadEventId));
    } else if (controlPending) {
      length = snprintf(payload, sizeof(payload), "event: control\ndata: {\"kind\":\"TAKEOVER\",\"requestId\":%lu}\n\n",
                        static_cast<unsigned long>(controlRequestId));
    } else {
      const auto& p = presentation_;
      length = snprintf(payload, sizeof(payload),
        "event: presentation\ndata: {\"l\":\"%s\",\"m\":\"%s\",\"t\":%llu,\"r\":%llu,\"o\":%llu,\"x\":%s,\"p\":\"%s\",\"n\":%u,\"c\":%u,\"g\":%s}\n\n",
        lifecycle(p.lifecycle), setupMode(p.sessionMode),
        (unsigned long long)systemTime(),
        (unsigned long long)p.remainingDuration, (unsigned long long)p.overtime,
        p.durationExpired ? "true" : "false", startPhase(p.startPhase),
        unsigned(p.redLightsLit), unsigned(p.redLightCount),
        p.startLightsGreen ? "true" : "false");
    }
    char frame[256];
    int frameLength = 0;
    if (length > 0) {
      const int header = snprintf(frame, sizeof(frame), "%x\r\n", unsigned(length));
      if (header > 0 && size_t(header + length + 2) < sizeof(frame)) {
        memcpy(frame + header, payload, size_t(length));
        frame[header + length] = '\r';
        frame[header + length + 1] = '\n';
        frameLength = header + length + 2;
      }
    }
    const int sent = frameLength > 0 ? httpd_socket_send(handle, fd, frame, size_t(frameLength), 0) : -1;
    if (sent < 0 || sent != frameLength) {
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
      Serial.printf("[SSE] send_failed revision=%lu lap_entry=%lu event_id=%lu at_us=%llu result=%d\n",
        static_cast<unsigned long>(revision),
        static_cast<unsigned long>(lapNoticeRevision_ == revision ? lapNoticeEntry_ : 0),
        static_cast<unsigned long>(lapNoticeRevision_ == revision ? lapNoticeEventId_ : 0),
        static_cast<unsigned long long>(systemTime()), sent);
#endif
      session->valid = false;
      if (sseSession_ == session) {
        sseSession_ = nullptr;
        eventConnected_ = false;
        eventGeneration_ = 0;
      }
      httpd_sess_trigger_close(handle, fd);
    } else {
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
      Serial.printf("[SSE] sent generation=%lu revision=%lu lap_entry=%lu lap=%lu event_id=%lu at_us=%llu\n",
        static_cast<unsigned long>(generation),
        static_cast<unsigned long>(revision),
        static_cast<unsigned long>(lapNoticeRevision_ == revision ? lapNoticeEntry_ : 0),
        static_cast<unsigned long>(lapNoticeRevision_ == revision ? lapNoticeNumber_ : 0),
        static_cast<unsigned long>(lapNoticeRevision_ == revision ? lapNoticeEventId_ : 0),
        static_cast<unsigned long long>(systemTime()));
#endif
      if (noticePending) eventSentRevision_ = revision;
      if (!noticePending && !controlPending && !keepalive && presentationSequence == presentationSequence_) {
        presentationSentSequence_ = presentationSequence;
      }
      if (controlPending && controlRequestId == controlNotificationRequestId_) {
        controlNotificationPending_ = false;
      }
      eventLastSendMs_ = millis();
      if (sseSession_ != session || !session->valid || eventGeneration_ != generation) {
        session->valid = false;
        if (sseSession_ == session) {
          sseSession_ = nullptr;
          eventConnected_ = false;
          eventGeneration_ = 0;
        }
      } else {
        (void)httpd_sess_update_lru_counter(handle, fd);
      }
    }
    eventWorkQueued_ = false;
  }
  static esp_err_t events(httpd_req_t* request) {
    RequestTrace trace(instance(), "/events");
    const uint64_t browserToken = client(request);
    BrowserInterface* browser = instance();
    const int fd = httpd_req_to_sockfd(request);
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
    Serial.printf("[SSE] handler_enter fd=%d revision=%lu at_us=%llu\n",
      fd, static_cast<unsigned long>(browser ? browser->noticeRevision_ : 0),
      static_cast<unsigned long long>(systemTime()));
#endif
    if (!browser || fd < 0) { trace.complete(ESP_ERR_INVALID_ARG); return ESP_FAIL; }
    if (browser->sseSession_ && browser->sseSession_->valid && browser->sseSession_->clientToken != browserToken) {
      httpd_resp_set_status(request, "409 Conflict");
      trace.complete(ESP_ERR_INVALID_STATE);
      return ESP_FAIL;
    }
    if (browser->sseSession_) {
      browser->controlDisconnected(browser->sseSession_->clientToken);
      browser->sseSession_->valid = false;
      httpd_sess_trigger_close(browser->sseSession_->server, browser->sseSession_->fd);
      browser->sseSession_ = nullptr;
      browser->eventConnected_ = false;
      browser->eventGeneration_ = 0;
    }
    httpd_resp_set_type(request, "text/event-stream");
    httpd_resp_set_hdr(request, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(request, "Connection", "keep-alive");
    char initial[128];
    const int length = snprintf(initial, sizeof(initial), "retry: 2000\ndata: {\"revision\":%lu}\n\n",
                                (unsigned long)browser->noticeRevision_);
 #if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
    Serial.printf("[SSE] initial_send fd=%d bytes=%d at_us=%llu\n", fd, length,
      static_cast<unsigned long long>(systemTime()));
 #endif
    const esp_err_t sent = length > 0 ? httpd_resp_send_chunk(request, initial, size_t(length)) : ESP_FAIL;
    if (sent != ESP_OK) {
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
      Serial.printf("[SSE] initial_result fd=%d result=%d at_us=%llu\n", fd, int(sent),
        static_cast<unsigned long long>(systemTime()));
#endif
      trace.complete(sent); return ESP_FAIL;
    }
    // Keep the chunked response open for the EventSource client.  Subsequent
    // notifications are sent from the HTTPD work queue, never by blocking
    // this request handler.  The context also marks this socket as the
    // browser's asynchronous notification session for diagnostics.
    auto* session = new SseSession{};
    if (!session) { trace.complete(ESP_ERR_NO_MEM); return ESP_FAIL; }
    session->owner = browser;
    session->server = browser->server_;
    session->fd = fd;
    session->clientToken = browserToken;
    session->generation = ++browser->nextEventGeneration_;
    if (session->generation == 0) session->generation = ++browser->nextEventGeneration_;
    session->valid = true;
    httpd_sess_set_ctx(browser->server_, fd, session, &BrowserInterface::freeSseSession);
    browser->sseSession_ = session;
    browser->eventConnected_ = true;
    browser->eventGeneration_ = session->generation;
    browser->eventSentRevision_ = browser->noticeRevision_;
    browser->eventLastSendMs_ = millis();
    browser->controlConnected(browserToken);
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
    Serial.printf("[SSE] async_session fd=%d generation=%lu revision=%lu at_us=%llu\n", fd,
      static_cast<unsigned long>(session->generation),
      static_cast<unsigned long>(browser->noticeRevision_),
      static_cast<unsigned long long>(systemTime()));
    Serial.printf("[SSE] connected revision=%lu at_us=%llu\n",
      static_cast<unsigned long>(browser->noticeRevision_),
      static_cast<unsigned long long>(systemTime()));
#endif
    trace.complete(ESP_OK);
    return ESP_OK;
  }
  static esp_err_t fact(httpd_req_t* request) {
    RequestTrace trace(instance(), "/fact");
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
    const esp_err_t sent = httpd_resp_sendstr(request, json); trace.complete(sent); return sent;
  }
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
  static esp_err_t browserLapDisplayed(httpd_req_t* request) {
    RequestTrace trace(instance(), "/diag/lap-displayed");
    if (request->content_len <= 0 || request->content_len >= 1024) {
      httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "lap display report required");
      trace.complete(ESP_ERR_INVALID_ARG);
      return ESP_FAIL;
    }
    char* body = static_cast<char*>(malloc(static_cast<size_t>(request->content_len) + 1U));
    if (!body) {
      httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "lap display report unavailable");
      trace.complete(ESP_ERR_NO_MEM);
      return ESP_FAIL;
    }
    memset(body, 0, static_cast<size_t>(request->content_len) + 1U);
    int received = 0;
    while (received < request->content_len) {
      const int part = httpd_req_recv(request, body + received, request->content_len - received);
      if (part <= 0) {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "lap display report unreadable");
        trace.complete(ESP_FAIL);
        free(body);
        return ESP_FAIL;
      }
      received += part;
    }
    const auto number = [&](const char* key, uint32_t fallback = 0U) {
      const char* p = strstr(body, key);
      const char* colon = p ? strchr(p, ':') : nullptr;
      return colon ? static_cast<uint32_t>(strtoul(colon + 1, nullptr, 10)) : fallback;
    };
    const uint32_t entry = number("\"entry\"");
    const uint32_t lane = number("\"lane\"");
    const uint32_t lap = number("\"lap\"");
    const uint32_t revision = number("\"revision\"");
    const uint32_t eventId = number("\"eventId\"");
    const auto number64 = [&](const char* key, uint64_t fallback = 0ULL) {
      const char* p = strstr(body, key);
      const char* colon = p ? strchr(p, ':') : nullptr;
      return colon ? static_cast<uint64_t>(strtoull(colon + 1, nullptr, 10)) : fallback;
    };
    const uint64_t serverUs = number64("\"serverUs\"");
    const uint64_t sseReceivedMs = number64("\"sseReceivedMs\"");
    const uint64_t refreshStartedMs = number64("\"refreshStartedMs\"");
    const uint64_t stateStartMs = number64("\"stateStartMs\"");
    const uint64_t stateEndMs = number64("\"stateEndMs\"");
    const uint64_t proposalStartMs = number64("\"proposalStartMs\"");
    const uint64_t proposalEndMs = number64("\"proposalEndMs\"");
    const uint64_t contextStartMs = number64("\"contextStartMs\"");
    const uint64_t contextEndMs = number64("\"contextEndMs\"");
    const uint64_t appliedMs = number64("\"appliedMs\"");
    const uint64_t renderStartedMs = number64("\"renderStartedMs\"");
    const uint64_t domUpdatedMs = number64("\"domUpdatedMs\"");
    const uint64_t displayedAt = systemTime();
    BrowserInterface* browser = instance();
    bool matched = false;
    for (auto& pending : browser->lapDisplayPending_) {
      if (!pending.active || pending.entry != entry || pending.lap != lap) continue;
      const uint64_t elapsed = displayedAt >= pending.passUs ? displayedAt - pending.passUs : 0;
      Serial.printf("[MEASURE] TRACE lane=%lu entry=%lu lap=%lu revision=%lu event_id=%lu server_sse_us=%llu http_recv_us=%llu sse_ms=%llu refresh_ms=%llu state_ms=%llu-%llu proposal_ms=%llu-%llu context_ms=%llu-%llu applied_ms=%llu render_ms=%llu dom_ms=%llu elapsed_ms=%llu\n",
        static_cast<unsigned long>(lane), static_cast<unsigned long>(entry), static_cast<unsigned long>(lap),
        static_cast<unsigned long>(revision), static_cast<unsigned long>(eventId),
        static_cast<unsigned long long>(serverUs), static_cast<unsigned long long>(displayedAt),
        static_cast<unsigned long long>(sseReceivedMs), static_cast<unsigned long long>(refreshStartedMs),
        static_cast<unsigned long long>(stateStartMs), static_cast<unsigned long long>(stateEndMs),
        static_cast<unsigned long long>(proposalStartMs), static_cast<unsigned long long>(proposalEndMs),
        static_cast<unsigned long long>(contextStartMs), static_cast<unsigned long long>(contextEndMs),
        static_cast<unsigned long long>(appliedMs), static_cast<unsigned long long>(renderStartedMs),
        static_cast<unsigned long long>(domUpdatedMs), static_cast<unsigned long long>(elapsed / 1000ULL));
      pending.active = false;
      matched = true;
      break;
    }
    if (!matched) {
      Serial.printf("[MEASURE] BROWSER DISPLAYED unmatched lane=%lu entry=%lu lap=%lu revision=%lu event_id=%lu at_us=%llu\n",
        static_cast<unsigned long>(lane), static_cast<unsigned long>(entry), static_cast<unsigned long>(lap),
        static_cast<unsigned long>(revision), static_cast<unsigned long>(eventId),
        static_cast<unsigned long long>(displayedAt));
    }
    free(body);
    httpd_resp_set_type(request, "application/json");
    const esp_err_t sent = httpd_resp_sendstr(request, matched ? "{\"accepted\":true}" : "{\"accepted\":false}");
    trace.complete(sent);
    return sent;
  }
#endif
  static bool correlationFromBody(httpd_req_t* request, uint32_t& correlation, SessionMode* requestedMode=nullptr, uint16_t* durationMinutes=nullptr, uint8_t* finishPolicy=nullptr, uint64_t* traceId=nullptr) {
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
    if (traceId) { const char* t = strstr(body, "\"diagnosticTraceId\""); const char* tc = t ? strchr(t, ':') : nullptr; *traceId = tc ? strtoull(tc + 1, nullptr, 10) : 0; }
    if(requestedMode){const char* mode=strstr(body,"OPEN_PRACTICE");const char* endurance=strstr(body,"ENDURANCE");*requestedMode=endurance?SessionMode::Endurance:(mode?SessionMode::OpenPractice:SessionMode::LapRace);}
    if(durationMinutes){const char* p=strstr(body,"durationMinutes");*durationMinutes=p?uint16_t(strtoul(strchr(p,':')+1,nullptr,10)):0;}
    if(finishPolicy){const char* named=strstr(body,"COMPLETE_CURRENT_LAP");const char* full=strstr(body,"COMPLETE_FULL_RACE_DISTANCE");const char* raw=strstr(body,"finishPolicy");*finishPolicy=named?1:full?2:(raw?uint8_t(strtoul(strchr(raw,':')+1,nullptr,10)):0);}
    return true;
  }
  static bool setupFromBody(httpd_req_t* request,uint32_t& correlation,SessionMode& mode,uint32_t& lapTarget,uint16_t& duration,uint8_t& finish,uint32_t& revision,uint64_t* traceId=nullptr){
    if(request->content_len<=0||request->content_len>=512)return false;char body[512]{};int received=0;while(received<request->content_len){const int part=httpd_req_recv(request,body+received,request->content_len-received);if(part<=0)return false;received+=part;}
    const char* c=strstr(body,"\"correlationId\"");const char* cc=c?strchr(c,':'):nullptr;const char* m=strstr(body,"\"mode\"");const char* mc=m?strchr(m,':'):nullptr;const char* lp=strstr(body,"\"lapTarget\"");const char* lc=lp?strchr(lp,':'):nullptr;const char* du=strstr(body,"\"durationMinutes\"");const char* dc=du?strchr(du,':'):nullptr;const char* fp=strstr(body,"\"finishPolicy\"");const char* fc=fp?strchr(fp,':'):nullptr;const char* rv=strstr(body,"\"proposalRevision\"");const char* rc=rv?strchr(rv,':'):nullptr;
    if(!cc||!mc||!lc||!dc||!fc||!rc)return false;char* end=nullptr;const unsigned long cv=strtoul(cc+1,&end,10);if(!end||end==cc+1||cv==0||cv>UINT32_MAX)return false;correlation=uint32_t(cv);if(strstr(mc,"OPEN_PRACTICE"))mode=SessionMode::OpenPractice;else if(strstr(mc,"ENDURANCE"))mode=SessionMode::Endurance;else if(strstr(mc,"LAP_RACE"))mode=SessionMode::LapRace;else return false;lapTarget=uint32_t(strtoul(lc+1,nullptr,10));duration=uint16_t(strtoul(dc+1,nullptr,10));finish=uint8_t(strtoul(fc+1,nullptr,10));revision=uint32_t(strtoul(rc+1,nullptr,10));if(traceId){const char* t=strstr(body,"\"diagnosticTraceId\"");const char* tc=t?strchr(t,':'):nullptr;*traceId=tc?strtoull(tc+1,nullptr,10):0;}return true;
  }
  static diagnostic::OperationName diagnosticOperationName(const char* value) {
    if (!value) return diagnostic::OperationName::Unknown;
    if (strstr(value, "RESTART")) return diagnostic::OperationName::RestartRace;
    if (strstr(value, "START")) return diagnostic::OperationName::Start;
    if (strstr(value, "SETUP")) return diagnostic::OperationName::Setup;
    if (strstr(value, "PAUSE")) return diagnostic::OperationName::Pause;
    if (strstr(value, "RESUME")) return diagnostic::OperationName::Resume;
    if (strstr(value, "RESTART")) return diagnostic::OperationName::RestartRace;
    if (strstr(value, "RACE_AGAIN")) return diagnostic::OperationName::RaceAgain;
    if (strstr(value, "END_RACE")) return diagnostic::OperationName::EndRace;
    if (strstr(value, "END_SESSION")) return diagnostic::OperationName::EndSession;
    if (strstr(value, "SKIP")) return diagnostic::OperationName::SkipFinishDisplay;
    return diagnostic::OperationName::Unknown;
  }
  static esp_err_t browserActionRoute(httpd_req_t* request) {
    BrowserInterface* browser = instance();
    char body[256]{};
    if (request->content_len > 0 && request->content_len < int(sizeof(body))) {
      int received = 0;
      while (received < request->content_len) {
        const int part = httpd_req_recv(request, body + received, request->content_len - received);
        if (part <= 0) break;
        received += part;
      }
      const char* op = strstr(body, "\"operation\"");
      const char* opColon = op ? strchr(op, ':') : nullptr;
      const char* trace = strstr(body, "\"diagnosticTraceId\"");
      const char* traceColon = trace ? strchr(trace, ':') : nullptr;
      const char* corr = strstr(body, "\"correlationId\"");
      const char* corrColon = corr ? strchr(corr, ':') : nullptr;
      const uint32_t visible = corrColon ? uint32_t(strtoul(corrColon + 1, nullptr, 10)) : 0;
      const uint64_t traceId = traceColon ? strtoull(traceColon + 1, nullptr, 10) : 0;
      char operation[40]{};
      if (opColon) {
        const char* start = strchr(opColon, '"');
        if (start) { ++start; const char* end = strchr(start, '"'); if (end) { const size_t n = size_t(end - start) < sizeof(operation)-1 ? size_t(end - start) : sizeof(operation)-1; memcpy(operation, start, n); operation[n] = 0; } }
      }
      if (browser->diagnosticTrace_) {
        diagnostic::ClientId clientId{}; clientId.value = client(request);
        browser->diagnosticTrace_->observeBrowserAction(diagnosticOperationName(operation), visible, clientId, traceId, systemTime());
      }
    }
    httpd_resp_set_status(request, "204 No Content");
    return httpd_resp_send(request, nullptr, 0);
  }
  static esp_err_t browserObservationRoute(httpd_req_t* request) {
    RequestTrace trace(instance(), "/diag/browser-observation");
    BrowserInterface* browser = instance();
    const uint64_t owner = client(request);
    if (!browser->diagnosticTrace_ || request->content_len <= 0 || request->content_len >= 768) {
      httpd_resp_set_status(request, "204 No Content");
      const esp_err_t sent = httpd_resp_send(request, nullptr, 0);
      trace.complete(sent);
      return sent;
    }
    char* body = static_cast<char*>(malloc(static_cast<size_t>(request->content_len) + 1U));
    if (!body) {
      httpd_resp_set_status(request, "204 No Content");
      const esp_err_t sent = httpd_resp_send(request, nullptr, 0);
      trace.complete(sent);
      return sent;
    }
    memset(body, 0, static_cast<size_t>(request->content_len) + 1U);
    int received = 0;
    while (received < request->content_len) {
      const int part = httpd_req_recv(request, body + received, request->content_len - received);
      if (part <= 0) break;
      received += part;
    }
    const auto number = [&](const char* key, uint32_t fallback = 0U) {
      const char* p = strstr(body, key);
      const char* colon = p ? strchr(p, ':') : nullptr;
      return colon ? static_cast<uint32_t>(strtoul(colon + 1, nullptr, 10)) : fallback;
    };
    const auto number64 = [&](const char* key, uint64_t fallback = 0ULL) {
      const char* p = strstr(body, key);
      const char* colon = p ? strchr(p, ':') : nullptr;
      return colon ? static_cast<uint64_t>(strtoull(colon + 1, nullptr, 10)) : fallback;
    };
    const auto text = [&](const char* key, char* out, size_t capacity) {
      if (!out || capacity == 0) return;
      out[0] = 0;
      const char* p = strstr(body, key);
      const char* start = p ? strchr(p, '"') : nullptr;
      if (!start) return;
      ++start;
      const char* end = strchr(start, '"');
      if (!end) return;
      const size_t length = size_t(end - start) < capacity - 1 ? size_t(end - start) : capacity - 1;
      memcpy(out, start, length);
      out[length] = 0;
    };
    char kind[32]{};
    char lifecycle[20]{};
    char mode[20]{};
    char operation[32]{};
    char phase[24]{};
    text("\"kind\"", kind, sizeof(kind));
    text("\"lifecycle\"", lifecycle, sizeof(lifecycle));
    text("\"sessionMode\"", mode, sizeof(mode));
    text("\"operation\"", operation, sizeof(operation));
    text("\"phase\"", phase, sizeof(phase));
    const uint64_t traceId = number64("\"diagnosticTraceId\"");
    const uint32_t visible = number("\"correlationId\"");
    const uint32_t revision = number("\"revision\"");
    const uint32_t eventId = number("\"eventId\"");
    const uint32_t detail = number("\"detail\"");
    const uint32_t phaseCode = strcmp(phase, "COUNTDOWN") == 0 ? 1U :
      strcmp(phase, "RESUME_COUNTDOWN") == 0 ? 2U :
      strcmp(phase, "GO") == 0 ? 3U : 0U;
    const uint32_t observationDetail = detail ? detail : phaseCode;
    const uint32_t notificationType = strcmp(kind, "presentation") == 0 ? 2U : 1U;
    const uint8_t lifecycleCode = strcmp(lifecycle, "STARTING") == 0 ? 2 :
      strcmp(lifecycle, "RACING") == 0 ? 3 : strcmp(lifecycle, "PAUSED") == 0 ? 4 :
      strcmp(lifecycle, "FINISHED") == 0 ? 6 : strcmp(lifecycle, "READY") == 0 ? 1 : 0;
    const uint8_t modeCode = strcmp(mode, "OPEN_PRACTICE") == 0 ? 2 :
      strcmp(mode, "ENDURANCE") == 0 ? 3 : strcmp(mode, "LAP_RACE") == 0 ? 1 : 0;
    const diagnostic::OperationName op = diagnosticOperationName(operation);
    diagnostic::ClientId clientId{};
    clientId.value = owner;
    if (strcmp(kind, "notification") == 0 || strcmp(kind, "presentation") == 0) {
      browser->diagnosticTrace_->observeBrowserNotification(
          clientId, traceId, visible, revision, eventId,
          notificationType, op, systemTime());
    } else {
      diagnostic::EventKind event = diagnostic::EventKind::StateFetchStarted;
      if (strcmp(kind, "state_received") == 0) event = diagnostic::EventKind::StateReceived;
      else if (strcmp(kind, "state_applied") == 0) event = diagnostic::EventKind::StateApplied;
      else if (strcmp(kind, "rendered") == 0) event = diagnostic::EventKind::BrowserRender;
      else if (strcmp(kind, "reconstruction") == 0) event = diagnostic::EventKind::Reconstruction;
      else if (strcmp(kind, "state_fetch_started") != 0) event = diagnostic::EventKind::ResourceHealth;
      browser->diagnosticTrace_->observeBrowserState(
          event, clientId, traceId, visible, revision,
          lifecycleCode, modeCode, observationDetail, op, systemTime());
    }
    free(body);
    httpd_resp_set_status(request, "204 No Content");
    const esp_err_t sent = httpd_resp_send(request, nullptr, 0);
    trace.complete(sent);
    return sent;
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
  static const char* controlResultText(ControlLeaseResult result) {
    switch (result) {
      case ControlLeaseResult::Accepted: return "ACCEPTED";
      case ControlLeaseResult::Pending: return "PENDING";
      case ControlLeaseResult::AlreadyOwner: return "ALREADY_OWNER";
      case ControlLeaseResult::AlreadyPending: return "ALREADY_PENDING";
      case ControlLeaseResult::Refused: return "REFUSED";
      case ControlLeaseResult::NoPending: return "NONE";
      default: return "REJECTED";
    }
  }
  static esp_err_t takeControlRoute(httpd_req_t* request) {
    BrowserInterface* browser = instance();
    const uint64_t owner = client(request);
    uint32_t requestId = 0;
    const ControlLeaseResult result = browser->requestControl(owner, requestId);
    const bool accepted = result == ControlLeaseResult::Accepted || result == ControlLeaseResult::AlreadyOwner;
    char json[160];
    snprintf(json, sizeof(json), "{\"result\":\"%s\",\"requestId\":%lu,\"hasMaster\":%s}",
             controlResultText(result), static_cast<unsigned long>(requestId),
             browser->hasMaster() ? "true" : "false");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_status(request, accepted ? "200 OK" : (result == ControlLeaseResult::Pending ? "202 Accepted" : "409 Conflict"));
    return httpd_resp_sendstr(request, json);
  }
  static esp_err_t allowControlRoute(httpd_req_t* request) {
    BrowserInterface* browser = instance();
    const uint64_t owner = client(request);
    uint32_t requestId = 0;
    const ControlLeaseResult result = browser->respondControl(owner, true, requestId);
    char json[128];
    snprintf(json, sizeof(json), "{\"result\":\"%s\",\"requestId\":%lu}",
             controlResultText(result), static_cast<unsigned long>(requestId));
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_status(request, result == ControlLeaseResult::Accepted ? "200 OK" : "409 Conflict");
    return httpd_resp_sendstr(request, json);
  }
  static esp_err_t keepControlRoute(httpd_req_t* request) {
    BrowserInterface* browser = instance();
    const uint64_t owner = client(request);
    uint32_t requestId = 0;
    const ControlLeaseResult result = browser->respondControl(owner, false, requestId);
    char json[128];
    snprintf(json, sizeof(json), "{\"result\":\"%s\",\"requestId\":%lu}",
             controlResultText(result), static_cast<unsigned long>(requestId));
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_status(request, result == ControlLeaseResult::Refused ? "200 OK" : "409 Conflict");
    return httpd_resp_sendstr(request, json);
  }
  static esp_err_t contextRoute(httpd_req_t* request) {
    RequestTrace trace(instance(), "/context");
    const uint64_t owner = client(request);
    char json[320];
    const ControlLeaseResult result = instance()->controlResultFor(owner);
    snprintf(json, sizeof(json), "{\"role\":\"%s\",\"hasMaster\":%s,\"isMaster\":%s,\"ownerConnected\":%s,\"takeoverPending\":%s,\"takeoverRequestedByThis\":%s,\"takeoverRequestId\":%lu,\"takeoverResult\":\"%s\"}",
             instance()->context(owner) == ClientContext::RaceDirectorSmug ? "Race Director" : "Spectator",
             instance()->hasMaster() ? "true" : "false",
             instance()->isControlOwner(owner) ? "true" : "false",
             instance()->controlOwnerConnected() ? "true" : "false",
             instance()->controlTakeoverPending() ? "true" : "false",
             instance()->controlTakeoverRequestedBy(owner) ? "true" : "false",
             static_cast<unsigned long>(instance()->controlTakeoverPending() ? instance()->controlTakeoverRequestId() : 0),
             controlResultText(result));
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    const esp_err_t sent = httpd_resp_sendstr(request, json); trace.complete(sent); return sent;
  }
  static esp_err_t startRoute(httpd_req_t* request) {
    uint32_t correlation = 0; uint64_t traceId = 0;
    if (!correlationFromBody(request, correlation, nullptr, nullptr, nullptr, &traceId)) {
      httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "correlationId required");
      return ESP_FAIL;
    }
    const uint64_t owner = client(request);
    if (!instance()->submitStart(correlation, instance()->context(owner), owner, SessionMode::None, 0, 0, traceId)) {
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
  static esp_err_t proposalRoute(httpd_req_t* request){
    RequestTrace trace(instance(), "/proposal");
    client(request);BrowserInterface* browser=instance();char* json=static_cast<char*>(malloc(4096));size_t length=0;if(!json){httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"proposal buffer unavailable");trace.complete(ESP_ERR_NO_MEM);return ESP_FAIL;}if(!serializeProposal(browser->proposedRaceSetup(),browser->controllerEpoch_,browser->proposalRevision(),browser->readiness(),json,4096,length)){free(json);httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"proposal unavailable");trace.complete(ESP_ERR_INVALID_SIZE);return ESP_FAIL;}httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");const esp_err_t sent=httpd_resp_sendstr(request,json);free(json);trace.complete(sent);return sent;
  }
  static esp_err_t practiceSummaryRoute(httpd_req_t* request){
    RequestTrace trace(instance(), "/practice-summary");client(request);BrowserInterface* browser=instance();char* json=static_cast<char*>(malloc(2048));size_t length=0;if(!json){httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"practice summary buffer unavailable");trace.complete(ESP_ERR_NO_MEM);return ESP_FAIL;}if(!serializePracticeSummary(browser->noticeboard_.practiceSummary(),json,2048,length)){free(json);httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"practice summary unavailable");trace.complete(ESP_ERR_INVALID_SIZE);return ESP_FAIL;}httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");const esp_err_t sent=httpd_resp_sendstr(request,json);free(json);trace.complete(sent);return sent;
  }
  static esp_err_t finishDisplayRoute(httpd_req_t* request){
    RequestTrace trace(instance(), "/finish-display");client(request);const NoticeboardState value=instance()->current();char json[256];snprintf(json,sizeof(json),"{\"active\":%s,\"until\":%llu,\"remainingUs\":%llu,\"durationSeconds\":%u,\"resultFinal\":%s}",value.finishDisplayActive?"true":"false",(unsigned long long)value.finishDisplayUntil,(unsigned long long)value.finishDisplayRemaining,unsigned(value.finishDisplayDurationSeconds),value.resultSealed?"true":"false");httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");const esp_err_t sent=httpd_resp_sendstr(request,json);trace.complete(sent);return sent;
  }
  static esp_err_t finishDisplaySkipRoute(httpd_req_t* request){return operationRoute(request,SessionOperation::SkipFinishDisplay);}
  static esp_err_t setupRoute(httpd_req_t* request){
    uint32_t correlation=0,lapTarget=0,revision=0;uint64_t traceId=0;SessionMode mode=SessionMode::None;uint16_t duration=0;uint8_t finish=0;if(!setupFromBody(request,correlation,mode,lapTarget,duration,finish,revision,&traceId)){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"complete proposal required");return ESP_FAIL;}const uint64_t owner=client(request);if(!instance()->submitSetup(correlation,instance()->context(owner),owner,mode,lapTarget,duration,finish,revision,traceId)){httpd_resp_set_status(request,"503 Service Unavailable");return httpd_resp_sendstr(request,"request unavailable");}char json[96];snprintf(json,sizeof(json),"{\"submitted\":true,\"correlationId\":%lu}",(unsigned long)correlation);httpd_resp_set_status(request,"202 Accepted");httpd_resp_set_type(request,"application/json");return httpd_resp_sendstr(request,json);
  }
  static esp_err_t operationRoute(httpd_req_t* request, SessionOperation operation, bool confirmed=false) {
    uint32_t correlation=0;uint64_t traceId=0;if(!correlationFromBody(request,correlation,nullptr,nullptr,nullptr,&traceId)){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"correlationId required");return ESP_FAIL;}
    const uint64_t owner=client(request);
    if(!instance()->submitOperation(correlation,operation,instance()->context(owner),owner,confirmed,traceId)){httpd_resp_set_status(request,"503 Service Unavailable");return httpd_resp_sendstr(request,"request unavailable");}
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
#if defined(PP_STAGE11_DEMO) || defined(PP_STAGE11_ACCEPTANCE) || defined(PP_STAGE12_DEMO) || defined(PP_STAGE12_ACCEPTANCE) || defined(PP_STAGE13_DEMO) || defined(PP_STAGE13_ACCEPTANCE) || defined(PP_STAGE14A_DEMO) || defined(PP_STAGE14A_ACCEPTANCE) || defined(PP_STAGE14B_DEMO) || defined(PP_STAGE14B_ACCEPTANCE) || defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
    char body[64]{}; int n=httpd_req_recv(request,body,sizeof(body)-1); if(n<=0){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"fixture action required");return ESP_FAIL;} body[n]=0;
    if(instance()->context(client(request))!=ClientContext::RaceDirectorSmug){httpd_resp_send_err(request,HTTPD_403_FORBIDDEN,"Race Director required");return ESP_FAIL;}
    if(strstr(body,"lane1")&&instance()->fixturePass_){if(!instance()->fixturePass_(1)){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"An active session is required before triggering a simulated car.");return ESP_FAIL;}}
    else if(strstr(body,"lane2")&&instance()->fixturePass_){if(!instance()->fixturePass_(2)){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"An active session is required before triggering a simulated car.");return ESP_FAIL;}}
    else if(strstr(body,"reset")&&instance()->fixtureReset_) instance()->fixtureReset_();
    else if(strstr(body,"target3")){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"session proposal must be changed through /request/setup");return ESP_FAIL;}
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
    RequestTrace trace(instance(), "/");
    client(request);
    static const char html[] = R"HTML(<!doctype html><meta charset="utf-8"><title>P&amp;P diagnostic Browser</title><style>body{font:16px system-ui;max-width:52rem;margin:2rem auto;padding:0 1rem}pre{background:#eee;padding:1rem;white-space:pre-wrap}.summary{background:#f5f5f5;padding:1rem;border-radius:.35rem;line-height:1.5}.status{font-size:1.3rem;font-weight:600;margin:.5rem 0}button{font:inherit;padding:.5rem;margin:.15rem}input:disabled,select:disabled{background:#eee;color:#555;border:2px solid #888;opacity:.7;cursor:not-allowed}.muted{color:#555}</style><h1>P&amp;P diagnostic Browser</h1><p id="connection">connecting</p><p id="role"></p><div id="human" class="summary">Loading race status...</div><p id="request"></p><button id="bootstrap">Make this Browser Race Director</button><button id="start">START</button><button id="pause">PAUSE</button><button id="honour">HONOUR RESTART</button><button id="grid">GRID RESTART</button><button id="raceAgain">RACE AGAIN</button><button id="restartRace">RESTART RACE</button><button id="endRace">END RACE</button><button id="results">RESULTS</button><button id="details">DETAILS</button><button id="history">HISTORY</button><button id="back">BACK</button><button id="home">HOME</button><button id="target3">NEXT RACE: 3 LAPS</button><div id="resultView" class="summary" hidden></div><h2>SIMULATED DETECTORS</h2><button id="lane1">LANE 1 LAP</button><button id="lane2">LANE 2 LAP</button><button id="resetTest">RESET TEST</button><h2>Engineering State</h2><pre id="state"></pre><h2>Latest Fact</h2><pre id="fact"></pre><script>let revision=null,hasState=false,nextCorrelation=1,polling=false,lastState=null;const $=x=>document.querySelector(x);const requestTimeoutMs=4000;async function get(path){const controller=new AbortController();const timeout=setTimeout(()=>controller.abort(),requestTimeoutMs);try{const r=await fetch(path,{cache:'no-store',signal:controller.signal});if(r.status===204)return null;if(!r.ok)throw Error(r.status);return r.json()}finally{clearTimeout(timeout)}}function sec(us){return (Number(us||0)/1000000).toFixed(2)+' s'}function life(v){return ({READY:'Ready',STARTING:'Starting',RACING:'Racing',PAUSED:'Paused',RESTARTING:'Restarting',FINISHED:'Finished',FAULTED:'Faulted'})[v]||v}function meth(v){return v==='HONOUR'?'Honour Restart':v==='GRID'?'Grid Restart':'None'}function clock(us){const total=Math.max(0,Math.floor(Number(us||0)/1000000));return String(Math.floor(total/60)).padStart(2,'0')+':'+String(total%60).padStart(2,'0')}function overtimeClock(us){const totalTenths=Math.max(0,Math.floor(Number(us||0)/100000));const totalSeconds=Math.floor(totalTenths/10);const minutes=Math.floor(totalSeconds/60);const seconds=totalSeconds-minutes*60;const fraction=totalTenths-totalSeconds*10;return '+'+String(minutes).padStart(2,'0')+':'+String(seconds).padStart(2,'0')+'.'+String(fraction)}function render(){const v=lastState;if(!v)return;let h='<div class="status">'+life(v.lifecycle)+'</div>';if(v.lifecycle==='PAUSED')h+='<div>Paused at the authoritative P&amp;P time.</div>';if(v.lifecycle==='RESTARTING')h+='<div>'+meth(v.restartMethod)+' - restart countdown active.</div>';if(v.lifecycle==='RACING')h+=v.sessionMode==='OPEN_PRACTICE'?'<div>Open Practice is running  -  no official result.</div>':v.sessionMode==='ENDURANCE'?'<div>Endurance is running.</div>':'<div>Race is running.</div>';if(v.lifecycle==='FINISHED')h+='<div>Race finished.</div>';h+='<div>'+v.entries.map((e,i)=>'<b>Lane '+e.lane+':</b> '+(e.laps||0)+' laps').join('; ')+'.</div>';if(v.hasLap)h+='<div>Last completed lap: '+sec(v.lastLapTime)+'</div>';if(v.sessionMode==='ENDURANCE'&&v.lifecycle!=='READY'){const displayRemaining=v.lifecycle==='STARTING'?(Number(v.remainingDuration||0)||Number(v.durationMinutes||0)*60000000):Number(v.remainingDuration||0);h+='<div class="status">Time remaining: '+clock(displayRemaining)+(v.lifecycle==='STARTING'?' (starts at GO)':'')+'</div>';if(v.finishBehaviour==='COMPLETE_CURRENT_LAP'&&Number(v.overtime||0)>0)h+='<div class="status">Overtime: '+overtimeClock(v.overtime)+'</div>';}h+='<div class="muted">Race integrity: '+(v.raceIntegrity==='OK'?'OK':'FAULTED')+'; results: '+(v.resultValid?'valid':'invalid')+'</div>';if(v.resultSealed){if(v.sessionMode==='ENDURANCE'){h+='<div><b>Endurance complete.</b> '+(v.durationExpired?'Duration expired at 00:00.':'Session settled.')+' <b>fastest lap:</b> '+(v.fastestLap?sec(v.fastestLap):'none')+'</div>';}else h+='<div><b>Winning time:</b> '+sec(v.winningTime)+'; <b>finish settled:</b> '+sec(v.finishTime)+'; <b>fastest lap:</b> '+(v.fastestLap?sec(v.fastestLap):'none')+'</div>';}if(v.scheduledRestartAt&&v.lifecycle==='RESTARTING')h+='<div id="countdown" class="status">Restart countdown</div>';$('#human').innerHTML=h;if(v.lifecycle==='RESTARTING'&&v.scheduledRestartAt){const deadline=performance.now()+3000;clearInterval(window.restartTimer);window.restartTimer=setInterval(()=>{const c=$('#countdown');if(!c||!lastState||lastState.lifecycle!=='RESTARTING'){clearInterval(window.restartTimer);return}const left=Math.max(0,(deadline-performance.now())/1000);c.textContent=left>0?'Restarting - '+left.toFixed(1)+' s remaining':'Restarting - GO'},100)}}
async function poll(){if(polling)return;polling=true;try{const oldRevision=revision;const n=await get('/noticeboard');const c=await get('/context');const state=await get('/state');const changed=!hasState||n.revision!==oldRevision;lastState=state;lastState.sampledAt=performance.now();$('#state').textContent=JSON.stringify(lastState,null,2);revision=n.revision;hasState=true;if(changed){try{$('#fact').textContent=JSON.stringify(await get('/fact'),null,2)}catch(e){}}if(window.reconcileModeFromState)window.reconcileModeFromState(lastState);render();$('#role').textContent=c.hasMaster?c.role:'No Race Director';window.browserHasMaster=!!c.hasMaster;$('#bootstrap').hidden=c.hasMaster;['start','pause','honour','grid'].forEach(id=>$('#'+id).disabled=!window.browserHasMaster);$('#raceAgain').disabled=!window.browserHasMaster||!lastState||lastState.lifecycle!=='FINISHED';$('#target3').disabled=!window.browserHasMaster||!lastState||lastState.lifecycle!=='READY';$('#connection').textContent='synchronised'}catch(e){$('#connection').textContent='unsynchronised';hasState=false}finally{polling=false}}
window.pendingMode='LAP_RACE';window.pendingModeDirty=false;window.modeSelectionListeners=[];window.onPendingModeChange=fn=>{window.modeSelectionListeners.push(fn);fn(window.pendingMode,window.pendingModeDirty)};window.setPendingMode=(mode,dirty)=>{window.pendingMode=mode;window.pendingModeDirty=!!dirty;window.modeSelectionListeners.forEach(fn=>fn(window.pendingMode,window.pendingModeDirty))};window.reconcileModeFromState=v=>{if(!v||v.lifecycle==='READY')return;const mode=v.sessionMode||'LAP_RACE';if(window.pendingModeDirty&&window.pendingMode===mode)window.pendingModeDirty=false;if(!window.pendingModeDirty)window.setPendingMode(mode,false)};window.startRequestWithBody=async(body,name)=>{const id=nextCorrelation++,started=performance.now();body.correlationId=id;const r=await fetch('/request/start',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});if(r.status!==202){$('#request').textContent='Request '+name+' was not accepted for submission.';return}const timer=setInterval(async()=>{const x=await get('/request-result?correlationId='+id);if(x){$('#request').textContent=(x.result==='ACCEPTED'?'Accepted: ':'Rejected: ')+(x.reason||'')+' ('+((performance.now()-started)/1000).toFixed(2)+' s; correlation '+id+')';clearInterval(timer)}},100)};window.dispatchStart=()=>{const mode=window.pendingMode||'LAP_RACE';if(mode==='OPEN_PRACTICE')return window.startRequestWithBody({mode:'OPEN_PRACTICE'},'START');if(mode==='ENDURANCE'){const p=window.enduranceProposalValues?window.enduranceProposalValues():{durationMinutes:1,finishPolicy:0};return window.startRequestWithBody({mode:'ENDURANCE',durationMinutes:p.durationMinutes,finishPolicy:p.finishPolicy},'START')}return operation('/request/start','START')};async function operation(path,name){const id=nextCorrelation++,started=performance.now();const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({correlationId:id})});if(r.status!==202){$('#request').textContent='Request '+name+' was not accepted for submission.';return}const timer=setInterval(async()=>{const x=await get('/request-result?correlationId='+id);if(x){$('#request').textContent=(x.result==='ACCEPTED'?'Accepted: ':'Rejected: ')+(x.reason||'')+' ('+((performance.now()-started)/1000).toFixed(2)+' s; correlation '+id+')';clearInterval(timer)}},100)}$('#bootstrap').onclick=async()=>{await fetch('/bootstrap',{method:'POST'});poll()};$('#start').onclick=()=>window.dispatchStart();$('#pause').onclick=()=>operation('/request/pause','PAUSE');$('#honour').onclick=()=>operation('/request/honour-restart','Honour Restart');$('#grid').onclick=()=>operation('/request/grid-restart','Grid Restart');$('#raceAgain').onclick=()=>operation('/request/race-again','Race Again');function rank(e){return(e.tied?'=':'')+e.rank}async function showResults(){const r=await get('/results');let h='<h2>Results</h2>';if(!r.sealed){h+='<p>No completed result.</p>'}else{if(r.mode==='ENDURANCE'){h+='<p>Endurance '+r.durationMinutes+' min; '+(r.finishBehaviour==='IMMEDIATE'?'Stop at Zero':'Finish Current Lap')+'. '+(r.expiryTime?'Duration boundary reached.':'')+'</p>';}else h+='<p>Winning time '+sec(r.winningTime)+'; finish settled '+sec(r.finishTime)+'.</p>';h+='<p>Fastest lap: '+(r.fastestLap?sec(r.fastestLap)+(r.fastestLapTied?' (tie)':''):'—')+'</p>';r.entries.forEach((e,i)=>h+='<p><b>'+rank(e)+' — Lane '+e.lane+'</b>: '+e.laps+' laps'+(e.classifiedLaps!==e.laps?' ('+e.classifiedLaps+' classified)':'')+(e.lapsBehind?' ('+e.lapsBehind+' behind)':'')+'; finish '+(e.completionTime?sec(e.completionTime):'—')+'; best lap '+(e.bestLap?sec(e.bestLap):'—')+'</p>')}$('#resultView').innerHTML=h;$('#resultView').hidden=false}async function showDetails(){const r=await get('/details');let h='<h2>Lap details</h2>';r.entries.forEach((e,i)=>{h+='<h3>Lane '+e.lane+' — entry '+e.raceEntryId+'</h3>';h+=e.records.length?'<ol>'+e.records.map(x=>'<li>Lap '+x.lapNumber+': '+sec(x.lapTime)+'</li>').join('')+'</ol>':'<p>No valid completed laps.</p>'});$('#resultView').innerHTML=h;$('#resultView').hidden=false}$('#results').onclick=showResults;$('#details').onclick=showDetails;$('#back').onclick=()=>{$('#resultView').hidden=true};$('#home').onclick=()=>{$('#resultView').hidden=true;$('#request').textContent='Home — race result remains available.'};async function fixture(action,name){const r=await fetch('/fixture',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action})});$('#request').textContent=r.ok?(action==='lane1'?'Lane 1 lap triggered':action==='lane2'?'Lane 2 lap triggered':action==='target3'?'Next race set to 3 laps.':'TEST RESET complete'):(action==='lane1'||action==='lane2'?'An active session is required before triggering a simulated car.':name+' unavailable')}$('#lane1').onclick=()=>fixture('lane1','Lane 1 passage');$('#lane2').onclick=()=>fixture('lane2','Lane 2 passage');$('#resetTest').onclick=()=>fixture('reset','TEST RESET');$('#target3').onclick=()=>fixture('target3','Next-race setup');poll();setInterval(()=>poll(),250)</script>)HTML";
    static const char destructiveControls[] = R"HTML(<script>async function destructive(open,confirmPath,label){const id=Date.now()%4294967295;const opened=await fetch(open,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({correlationId:id})});if(!opened.ok){document.querySelector('#request').textContent=label+' is unavailable.';return}if(!confirm('Confirm '+label+'?')){document.querySelector('#request').textContent=label+' cancelled.';return}const r=await fetch(confirmPath,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({correlationId:id})});document.querySelector('#request').textContent=r.status===202?label+' submitted.':label+' was not accepted for submission.'}document.querySelector('#restartRace').onclick=()=>destructive('/request/restart-race','/request/restart-race/confirm','Restart Race');document.querySelector('#endRace').onclick=()=>destructive('/request/end-race','/request/end-race/confirm','End Race');document.querySelector('#history').onclick=async()=>{const r=await fetch('/history');const h=await r.json();const v=document.querySelector('#resultView');const historyMode=x=>x.mode==='ENDURANCE'?'Endurance':x.mode==='OPEN_PRACTICE'?'Open Practice':'Lap Race';v.innerHTML='<h2>History</h2>'+(h.entries.length?h.entries.map(x=>{const rows=(x.entries||[]).map(e=>'Lane '+e.lane+': '+e.laps+' laps'+(e.classifiedLaps!==e.laps?' ('+e.classifiedLaps+' classified)':'')).join('; ');const end=x.mode==='ENDURANCE'?(' — '+x.durationMinutes+' min, '+(x.finishBehaviour==='IMMEDIATE'?'Stop at Zero':'Finish Current Lap')+(x.overtime?' — overtime':'')+(rows?'; '+rows:'')):(' — winning time '+(Number(x.winningTime)/1000000).toFixed(2)+' s');return '<p>Race '+x.sequence+': '+historyMode(x)+end+'</p>';}).join(''):'<p>No completed races.</p>');v.hidden=false};async function stage13Fact(){try{const f=await get('/fact');const old=document.querySelector('#stage13Fact');if(old)old.remove();if(f.type==='FALSE_START'){const msg=document.createElement('div');msg.id='stage13Fact';msg.className='status';msg.textContent='False start — Lane '+f.raceEntryId+' ('+f.policy+').';document.querySelector('#human').appendChild(msg)}}catch(e){}}function stage13Lights(){try{if(!lastState)return;const p=lastState.startPresentation||{};let line=document.querySelector('#stage13Lights');if(!line){line=document.createElement('div');line.id='stage13Lights';line.className='status';document.querySelector('#human').appendChild(line)}const phase=p.phase||'NONE';line.textContent=phase==='GO'?(p.green?'GO — GREEN':'GO — LIGHTS OUT'):phase==='COUNTDOWN'?'Start sequence — '+(Number(p.redLightsLit||0))+' of '+(Number(p.redLightCount||0))+' red lights':phase==='RESUME_COUNTDOWN'?'Resume sequence — '+(Number(p.redLightsLit||0))+' of '+(Number(p.redLightCount||0))+' red lights':'';}catch(e){}}setInterval(stage13Lights,100);setInterval(stage13Fact,300);</script>)HTML";
    static const char recordControls[] = R"HTML(<script>const recordButton=document.createElement('button');recordButton.id='records';recordButton.textContent='RECORDS';document.querySelector('#history').after(recordButton);function recordTime(v){return v?sec(v):'—'}async function showRecords(){const r=await get('/records');const v=document.querySelector('#resultView');v.innerHTML='<h2>Records — '+r.track+'</h2><p>Record era '+r.era+'. '+r.entries.map(e=>'Lane '+e.lane+' PB: '+recordTime(e.pb)).join('; ')+ '; Track Record: '+recordTime(r.trackRecord)+'.</p><p><button id="clearHistory">CLEAR HISTORY</button><button id="clearLane1">CLEAR LANE 1 PB</button><button id="clearLane2">CLEAR LANE 2 PB</button><button id="clearTrack">CLEAR TRACK RECORD</button><button id="clearAll">CLEAR ALL RECORDS</button></p>';v.hidden=false;document.querySelector('#clearHistory').onclick=()=>destructive('/request/clear-history','/request/clear-history/confirm','Clear History');document.querySelector('#clearLane1').onclick=()=>destructive('/request/clear-lane-1-records','/request/clear-lane-1-records/confirm','Clear Lane 1 PB');document.querySelector('#clearLane2').onclick=()=>destructive('/request/clear-lane-2-records','/request/clear-lane-2-records/confirm','Clear Lane 2 PB');document.querySelector('#clearTrack').onclick=()=>destructive('/request/clear-track-record','/request/clear-track-record/confirm','Clear Track Record');document.querySelector('#clearAll').onclick=()=>destructive('/request/clear-all-records','/request/clear-all-records/confirm','Clear All Records')}recordButton.onclick=showRecords;</script>)HTML";
    // The sampled browser monotonic instant is presentation-only.  It lets a
    // reconnecting Browser animate the already-authoritative schedule without
    // deciding GO or sending a timing value back to P&P.
    static const char stage13Presentation[] = R"HTML(<script>setInterval(()=>{try{if(lastState&&!lastState.sampledAt)lastState.sampledAt=performance.now()}catch(e){}},25);</script>)HTML";
#if defined(PP_STAGE14B_ACCEPTANCE) || defined(PP_STAGE14B_DEMO) || defined(PP_STAGE14C_ACCEPTANCE) || defined(PP_STAGE14C_DEMO)
    static const char authorityPresentation[] = R"HTML(<script>(function(){
const host=document.querySelector('#bootstrap');const box=document.createElement('div');box.id='authoritySetup';box.innerHTML='<h2>SESSION PROPOSAL</h2><button id="selectLap">SELECT LAP RACE</button><button id="selectPractice">SELECT OPEN PRACTICE</button><button id="selectEndurance">SELECT ENDURANCE</button><label> Minutes <input id="proposalMinutes" type="number" min="1" max="999" value="1"></label><label> Finish <select id="proposalFinish"><option value="0">Stop at Zero</option><option value="1">Finish Current Lap</option></select></label><button id="proposalResume">RESUME</button><button id="proposalEnd">END SESSION</button><p id="proposalStatus"></p>';host.before(box);
let proposal=null;let proposalRevision=0;let proposalPoll=0;const modeName=m=>m==='OPEN_PRACTICE'?'OPEN PRACTICE':m==='ENDURANCE'?'ENDURANCE':'LAP RACE';const setupButton=(id,mode)=>{const b=document.querySelector(id);b.onclick=()=>{if(window.browserHasMaster!==true){$('#request').textContent='Race Director authority required to change the session proposal.';return}if(!proposal)return;const d={correlationId:++nextCorrelation,mode,lapTarget:mode==='LAP_RACE'?(proposal.lapTarget||2):proposal.lapTarget||0,durationMinutes:mode==='ENDURANCE'?Math.max(1,Math.min(999,Number(document.querySelector('#proposalMinutes').value)||proposal.durationMinutes||1)):proposal.durationMinutes||1,finishPolicy:mode==='ENDURANCE'?Number(document.querySelector('#proposalFinish').value)||0:0,proposalRevision:proposalRevision};fetch('/request/setup',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)}).then(()=>window.poll());};return b};setupButton('#selectLap','LAP_RACE');setupButton('#selectPractice','OPEN_PRACTICE');setupButton('#selectEndurance','ENDURANCE');
const submitProposalEdit=()=>{if(window.browserHasMaster!==true||!proposal||lastState.lifecycle!=='READY')return;const mode=proposal.mode==='ENDURANCE'?'ENDURANCE':proposal.mode==='OPEN_PRACTICE'?'OPEN_PRACTICE':'LAP_RACE';const d={correlationId:++nextCorrelation,mode,lapTarget:proposal.lapTarget||0,durationMinutes:mode==='ENDURANCE'?Math.max(1,Math.min(999,Number(document.querySelector('#proposalMinutes').value)||proposal.durationMinutes||1)):proposal.durationMinutes||1,finishPolicy:mode==='ENDURANCE'?Number(document.querySelector('#proposalFinish').value)||0:0,proposalRevision:proposalRevision};fetch('/request/setup',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)}).then(()=>window.poll());};document.querySelector('#proposalMinutes').onchange=submitProposalEdit;document.querySelector('#proposalFinish').onchange=submitProposalEdit;
async function authorityPoll(){if(polling)return;polling=true;try{const n=await get('/noticeboard'),c=await get('/context'),s=await get('/state'),p=await get('/proposal');lastState=s;lastState.sampledAt=performance.now();proposal=p;proposalRevision=Number(p.proposalRevision||0);proposalPoll++;revision=n.revision;hasState=true;$('#state').textContent=JSON.stringify(s,null,2);$('#role').textContent=c.hasMaster?c.role:'No Race Director';window.browserHasMaster=!!c.hasMaster;$('#bootstrap').hidden=!!c.hasMaster;$('#connection').textContent='synchronised';$('#proposalStatus').textContent='Proposal '+modeName(p.mode)+'; '+(p.startable?'ready to start':p.readiness);document.querySelector('#selectLap').textContent=p.mode==='LAP_RACE'?'LAP RACE SELECTED':'SELECT LAP RACE';document.querySelector('#selectPractice').textContent=p.mode==='OPEN_PRACTICE'?'OPEN PRACTICE SELECTED':'SELECT OPEN PRACTICE';document.querySelector('#selectEndurance').textContent=p.mode==='ENDURANCE'?'ENDURANCE SELECTED':'SELECT ENDURANCE';document.querySelector('#proposalMinutes').value=String(p.durationMinutes||1);document.querySelector('#proposalFinish').value=p.finishBehaviour==='COMPLETE_CURRENT_LAP'?'1':'0';const active=s.lifecycle!=='READY';['selectLap','selectPractice','selectEndurance','proposalMinutes','proposalFinish'].forEach(id=>document.querySelector('#'+id).disabled=!window.browserHasMaster||active);document.querySelector('#start').disabled=!window.browserHasMaster||!p.startable;document.querySelector('#pause').disabled=!window.browserHasMaster||active&&s.lifecycle!=='RACING';document.querySelector('#proposalResume').disabled=!window.browserHasMaster||s.lifecycle!=='PAUSED'||p.mode!=='OPEN_PRACTICE';document.querySelector('#proposalEnd').disabled=!window.browserHasMaster||p.mode!=='OPEN_PRACTICE'||(s.lifecycle!=='RACING'&&s.lifecycle!=='PAUSED');document.querySelector('#target3').disabled=!window.browserHasMaster||active;render();}catch(e){$('#connection').textContent='unsynchronised';hasState=false}finally{polling=false}}
window.reconcileModeFromState=()=>{};window.poll=authorityPoll;window.dispatchStart=()=>operation('/request/start','START');document.querySelector('#start').onclick=window.dispatchStart;document.querySelector('#target3').onclick=()=>{if(!proposal)return;const id=++nextCorrelation;fetch('/request/setup',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({correlationId:id,mode:'LAP_RACE',lapTarget:3,durationMinutes:proposal.durationMinutes||1,finishPolicy:0,proposalRevision})}).then(()=>window.poll())};document.querySelector('#proposalResume').onclick=()=>operation('/request/resume','RESUME');document.querySelector('#proposalEnd').onclick=()=>operation('/request/end-session','END SESSION');document.querySelector('#lane1').onclick=()=>fixture('lane1','Lane 1 passage');document.querySelector('#lane2').onclick=()=>fixture('lane2','Lane 2 passage');document.querySelector('#resetTest').onclick=()=>fixture('reset','TEST RESET');authorityPoll();})();</script>)HTML";
#else
    static const char authorityPresentation[] = "";
#endif
    static const char stage14bPresentation[] = "";
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
    static const char stage14cPresentation[] = "";
#else
    static const char stage14cPresentation[] = "";
#endif
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    if (httpd_resp_send_chunk(request, html, sizeof(html) - 1) != ESP_OK) { trace.complete(ESP_FAIL); return ESP_FAIL; }
    if (httpd_resp_send_chunk(request, destructiveControls, sizeof(destructiveControls) - 1) != ESP_OK) { trace.complete(ESP_FAIL); return ESP_FAIL; }
    if (httpd_resp_send_chunk(request, recordControls, sizeof(recordControls) - 1) != ESP_OK) { trace.complete(ESP_FAIL); return ESP_FAIL; }
    if (httpd_resp_send_chunk(request, stage13Presentation, sizeof(stage13Presentation) - 1) != ESP_OK) { trace.complete(ESP_FAIL); return ESP_FAIL; }
    if (httpd_resp_send_chunk(request, stage14bPresentation, sizeof(stage14bPresentation) - 1) != ESP_OK) { trace.complete(ESP_FAIL); return ESP_FAIL; }
    if (httpd_resp_send_chunk(request, stage14cPresentation, sizeof(stage14cPresentation) - 1) != ESP_OK) { trace.complete(ESP_FAIL); return ESP_FAIL; }
    if (httpd_resp_send_chunk(request, authorityPresentation, sizeof(authorityPresentation) - 1) != ESP_OK) { trace.complete(ESP_FAIL); return ESP_FAIL; }
    const esp_err_t sent = httpd_resp_send_chunk(request, nullptr, 0); trace.complete(sent); return sent;
  }
  static esp_err_t bootstrapDiagnostic(httpd_req_t* request) {
    RequestTrace trace(instance(), "/diag/bootstrap");
    char query[64]{};
    char step[32] = "unknown";
    if (httpd_req_get_url_query_str(request, query, sizeof(query)) == ESP_OK) {
      httpd_query_key_value(query, "step", step, sizeof(step));
    }
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
    Serial.printf("[BOOTHTTP] browser_marker step=%s at_us=%llu\n", step,
                  static_cast<unsigned long long>(systemTime()));
#endif
    httpd_resp_set_status(request, "204 No Content");
    const esp_err_t sent = httpd_resp_send(request, nullptr, 0);
    trace.complete(sent);
    return sent;
  }
  static bool diagnosticControlAllowed(httpd_req_t* request, BrowserInterface* browser, uint64_t& owner) {
    owner = client(request);
    if (browser && browser->isControlOwner(owner)) return true;
    httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "controlling Race Director required");
    return false;
  }
  static const char* diagnosticResultText(diagnostic::RecorderResult result) {
    switch (result) {
      case diagnostic::RecorderResult::Accepted: return "ACCEPTED";
      case diagnostic::RecorderResult::AlreadyActive: return "ALREADY_ACTIVE";
      case diagnostic::RecorderResult::NotActive: return "NOT_ACTIVE";
      case diagnostic::RecorderResult::Fault: return "FAULT";
      default: return "REJECTED";
    }
  }
  static esp_err_t diagnosticStartRoute(httpd_req_t* request) {
    BrowserInterface* browser = instance();
    uint64_t owner = 0;
    if (!diagnosticControlAllowed(request, browser, owner)) return ESP_FAIL;
    if (!browser->diagnosticTrace_) {
      httpd_resp_set_status(request, "503 Service Unavailable");
      httpd_resp_sendstr(request, "diagnostics unavailable");
      return ESP_FAIL;
    }
    diagnostic::SessionStartRequest start{};
    start.controllerEpoch.value = browser->controllerEpoch_;
    start.client.value = owner;
    diagnostic::SessionStatus status{};
    const diagnostic::RecorderResult result = browser->diagnosticTrace_->recorder().start(start, status);
    char json[192];
    snprintf(json, sizeof(json), "{\"result\":\"%s\",\"session\":%llu,\"controllerEpoch\":%llu,\"lifecycle\":%u}",
             diagnosticResultText(result), static_cast<unsigned long long>(status.session.value),
             static_cast<unsigned long long>(status.controllerEpoch.value), static_cast<unsigned>(status.lifecycle));
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_status(request, result == diagnostic::RecorderResult::Accepted ? "202 Accepted" :
      (result == diagnostic::RecorderResult::AlreadyActive ? "409 Conflict" : "503 Service Unavailable"));
    return httpd_resp_sendstr(request, json);
  }
  static esp_err_t diagnosticStopRoute(httpd_req_t* request) {
    BrowserInterface* browser = instance();
    uint64_t owner = 0;
    if (!diagnosticControlAllowed(request, browser, owner)) return ESP_FAIL;
    if (!browser->diagnosticTrace_) {
      httpd_resp_set_status(request, "503 Service Unavailable");
      httpd_resp_sendstr(request, "diagnostics unavailable");
      return ESP_FAIL;
    }
    diagnostic::SessionStatus status{};
    const diagnostic::RecorderResult result = browser->diagnosticTrace_->recorder().stop(status);
    char json[192];
    snprintf(json, sizeof(json), "{\"result\":\"%s\",\"session\":%llu,\"lifecycle\":%u}",
             diagnosticResultText(result), static_cast<unsigned long long>(status.session.value), static_cast<unsigned>(status.lifecycle));
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_status(request, result == diagnostic::RecorderResult::Accepted ? "200 OK" : "409 Conflict");
    return httpd_resp_sendstr(request, json);
  }
  static esp_err_t diagnosticStatusRoute(httpd_req_t* request) {
    RequestTrace trace(instance(), "/diag/status");
    BrowserInterface* browser = instance();
    uint64_t owner = 0;
    if (!diagnosticControlAllowed(request, browser, owner)) { trace.complete(ESP_ERR_INVALID_STATE); return ESP_FAIL; }
    if (!browser->diagnosticTrace_) {
      httpd_resp_set_status(request, "503 Service Unavailable");
      httpd_resp_sendstr(request, "diagnostics unavailable");
      trace.complete(ESP_ERR_INVALID_STATE);
      return ESP_FAIL;
    }
    diagnostic::SessionStatus session{};
    diagnostic::StorageStatus storage{};
    size_t retained = 0;
    if (!browser->diagnosticTrace_->status(session, storage, retained)) {
      httpd_resp_set_status(request, "503 Service Unavailable");
      httpd_resp_sendstr(request, "diagnostic status unavailable");
      trace.complete(ESP_ERR_INVALID_STATE);
      return ESP_FAIL;
    }
    char* json = static_cast<char*>(malloc(1024));
    if (!json) {
      httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "diagnostic status buffer unavailable");
      trace.complete(ESP_ERR_NO_MEM);
      return ESP_FAIL;
    }
    const int length = snprintf(json, 1024,
      "{\"formatVersion\":1,\"controllerEpoch\":%llu,\"session\":%llu,\"lifecycle\":%u,\"storageHealth\":%u,\"backend\":%u,\"lastFailure\":%u,\"nextSequence\":%llu,\"bytesRecorded\":%llu,\"retainedRecords\":%u,\"droppedRecords\":%llu,\"evidenceVolatile\":%s,\"evidenceIncomplete\":%s,\"retrievalAvailable\":%s}",
      static_cast<unsigned long long>(session.controllerEpoch.value),
      static_cast<unsigned long long>(session.session.value),
      static_cast<unsigned>(session.lifecycle), static_cast<unsigned>(storage.health),
      static_cast<unsigned>(storage.backend), static_cast<unsigned>(storage.lastFailure),
      static_cast<unsigned long long>(session.nextSequence), static_cast<unsigned long long>(session.bytesRecorded), static_cast<unsigned>(retained),
      static_cast<unsigned long long>(session.droppedRecords), session.evidenceVolatile ? "true" : "false",
      session.evidenceIncomplete ? "true" : "false", retained ? "true" : "false");
    if (length < 0 || length >= 1024) {
      free(json);
      httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "diagnostic status serialization failed");
      trace.complete(ESP_ERR_INVALID_SIZE);
      return ESP_FAIL;
    }
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    const esp_err_t sent = httpd_resp_send(request, json, length);
    free(json);
    trace.complete(sent);
    return sent;
  }
  static esp_err_t diagnosticRecordsRoute(httpd_req_t* request) {
    RequestTrace trace(instance(), "/diag/records");
    BrowserInterface* browser = instance();
    uint64_t owner = 0;
    if (!diagnosticControlAllowed(request, browser, owner)) { trace.complete(ESP_ERR_INVALID_STATE); return ESP_FAIL; }
    if (!browser->diagnosticTrace_) {
      httpd_resp_set_status(request, "503 Service Unavailable");
      httpd_resp_sendstr(request, "diagnostics unavailable");
      trace.complete(ESP_ERR_INVALID_STATE);
      return ESP_FAIL;
    }
    char query[96]{};
    size_t cursor = 0;
    if (httpd_req_get_url_query_str(request, query, sizeof(query)) == ESP_OK) {
      char value[24]{};
      if (httpd_query_key_value(query, "cursor", value, sizeof(value)) == ESP_OK) {
        char* end = nullptr;
        const unsigned long parsed = strtoul(value, &end, 10);
        if (!end || *end || parsed > SIZE_MAX) {
          httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid diagnostic cursor");
          trace.complete(ESP_ERR_INVALID_ARG);
          return ESP_FAIL;
        }
        cursor = static_cast<size_t>(parsed);
      }
    }
    uint8_t* payload = static_cast<uint8_t*>(malloc(diagnostic::DiagnosticRetrieval::MaxPayloadBytes));
    char* json = static_cast<char*>(malloc(diagnostic::DiagnosticRetrieval::MaxPayloadBytes * 2U + 2048U));
    if (!payload || !json) {
      free(payload); free(json);
      httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "diagnostic retrieval buffer unavailable");
      trace.complete(ESP_ERR_NO_MEM);
      return ESP_FAIL;
    }
    diagnostic::SessionStatus session{};
    diagnostic::StorageStatus storage{};
    size_t retained = 0;
    browser->diagnosticTrace_->status(session, storage, retained);
    diagnostic::RetrievalChunk chunk{};
    const bool readOk = diagnostic::DiagnosticRetrieval::next(
      browser->diagnosticTrace_->recorder(), cursor, payload,
      diagnostic::DiagnosticRetrieval::MaxPayloadBytes, chunk);
    size_t used = 0;
    if (!appendJson(json, diagnostic::DiagnosticRetrieval::MaxPayloadBytes * 2U + 2048U, used,
        "{\"formatVersion\":1,\"cursor\":%u,\"nextCursor\":%u,\"retainedRecords\":%u,\"end\":%s,\"available\":%s,\"readFailed\":%s,\"evidenceIncomplete\":%s,\"session\":%llu,\"controllerEpoch\":%llu,\"payloadHex\":\"",
        static_cast<unsigned>(chunk.cursor), static_cast<unsigned>(chunk.nextCursor), static_cast<unsigned>(retained),
        chunk.end ? "true" : "false", chunk.available ? "true" : "false", chunk.readFailed ? "true" : "false",
        chunk.evidenceIncomplete ? "true" : "false", static_cast<unsigned long long>(session.session.value),
        static_cast<unsigned long long>(session.controllerEpoch.value))) {
      free(payload); free(json); httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "diagnostic retrieval serialization failed"); trace.complete(ESP_ERR_INVALID_SIZE); return ESP_FAIL;
    }
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < chunk.payloadBytes; ++i) {
      if (!appendJson(json, diagnostic::DiagnosticRetrieval::MaxPayloadBytes * 2U + 2048U, used,
                      "%c%c", hex[payload[i] >> 4], hex[payload[i] & 0x0F])) {
        free(payload); free(json); httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "diagnostic retrieval serialization failed"); trace.complete(ESP_ERR_INVALID_SIZE); return ESP_FAIL;
      }
    }
    if (chunk.available && !appendJson(json, diagnostic::DiagnosticRetrieval::MaxPayloadBytes * 2U + 2048U, used,
        "\",\"sequence\":%llu,\"kind\":%u,\"severity\":%u,\"timestamp\":%llu,\"client\":%llu,\"traceId\":%llu,\"requestCorrelation\":%u,\"eventCorrelation\":%u,\"parentCorrelation\":%u,\"payloadType\":%u,\"operation\":%u,\"payloadBytes\":%u}",
        static_cast<unsigned long long>(chunk.envelope.sequence), static_cast<unsigned>(chunk.envelope.kind),
        static_cast<unsigned>(chunk.envelope.severity), static_cast<unsigned long long>(chunk.envelope.timestamp),
        static_cast<unsigned long long>(chunk.envelope.client.value), static_cast<unsigned long long>(chunk.envelope.traceId),
        static_cast<unsigned>(chunk.envelope.correlation.request), static_cast<unsigned>(chunk.envelope.correlation.event),
        static_cast<unsigned>(chunk.envelope.correlation.parent), static_cast<unsigned>(chunk.envelope.payloadType),
        static_cast<unsigned>(chunk.envelope.operation), static_cast<unsigned>(chunk.payloadBytes))) {
      free(payload); free(json); httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "diagnostic retrieval serialization failed"); trace.complete(ESP_ERR_INVALID_SIZE); return ESP_FAIL;
    } else if (!chunk.available && !appendJson(json, diagnostic::DiagnosticRetrieval::MaxPayloadBytes * 2U + 2048U, used, "\"}")) {
      free(payload); free(json); httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "diagnostic retrieval serialization failed"); trace.complete(ESP_ERR_INVALID_SIZE); return ESP_FAIL;
    }
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    const esp_err_t sent = httpd_resp_send(request, json, used);
    free(payload); free(json);
    trace.complete(sent);
    (void)readOk;
    return sent;
  }
  static esp_err_t health(httpd_req_t* request) {
    RequestTrace trace(instance(), "/health");
    httpd_resp_set_type(request, "application/json");
    const esp_err_t sent = httpd_resp_sendstr(request, "{\"ok\":true,\"browser\":\"stage13\"}"); trace.complete(sent); return sent;
  }
  void startServer() {
    instance() = this;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 8192;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    // ESP32 Arduino's configured LWIP limit permits at most 13 sockets.
    // LRU eviction keeps sleeping presentation clients from exhausting them.
    config.max_open_sockets = 13;
    config.lru_purge_enable = true;
    // Stage 14B adds generic Resume/End Session and Practice-facing routes;
    // keep enough handler slots for the complete diagnostic Browser surface.
    config.max_uri_handlers = 48;
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
      {"/events", HTTP_GET, events, nullptr},
      {"/diag/bootstrap", HTTP_GET, bootstrapDiagnostic, nullptr},
      {"/diag/browser", HTTP_POST, browserActionRoute, nullptr},
      {"/diag/browser-observation", HTTP_POST, browserObservationRoute, nullptr},
      {"/diag/session/start", HTTP_POST, diagnosticStartRoute, nullptr},
      {"/diag/session/stop", HTTP_POST, diagnosticStopRoute, nullptr},
      {"/diag/status", HTTP_GET, diagnosticStatusRoute, nullptr},
      {"/diag/records", HTTP_GET, diagnosticRecordsRoute, nullptr},
      {"/proposal", HTTP_GET, proposalRoute, nullptr},
      {"/practice-summary", HTTP_GET, practiceSummaryRoute, nullptr},
      {"/finish-display", HTTP_GET, finishDisplayRoute, nullptr},
      {"/results", HTTP_GET, resultsRoute, nullptr},
      {"/details", HTTP_GET, detailsRoute, nullptr},
      {"/history", HTTP_GET, historyRoute, nullptr},
      {"/records", HTTP_GET, recordsRoute, nullptr},
      {"/fact", HTTP_GET, fact, nullptr},
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
      {"/diag/lap-displayed", HTTP_POST, browserLapDisplayed, nullptr},
#endif
      {"/health", HTTP_GET, health, nullptr},
      {"/context", HTTP_GET, contextRoute, nullptr},
      {"/bootstrap", HTTP_POST, bootstrapRoute, nullptr},
      {"/request/take-control", HTTP_POST, takeControlRoute, nullptr},
      {"/request/take-control/allow", HTTP_POST, allowControlRoute, nullptr},
      {"/request/take-control/keep", HTTP_POST, keepControlRoute, nullptr},
      {"/request/start", HTTP_POST, startRoute, nullptr},
      {"/request/setup", HTTP_POST, setupRoute, nullptr},
      {"/request/pause", HTTP_POST, pauseRoute, nullptr},
      {"/request/resume", HTTP_POST, resumeRoute, nullptr},
      {"/request/end-session", HTTP_POST, endSessionRoute, nullptr},
      {"/request/skip-finish-display", HTTP_POST, finishDisplaySkipRoute, nullptr},
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
    ++serverStarts_;
    serverStartError_ = 0;
  }
  void stopServer() {
    if (!server_) return;
    if (sseSession_) controlDisconnected(sseSession_->clientToken);
    if (sseSession_) sseSession_->valid = false;
    eventConnected_ = false;
    eventGeneration_ = 0;
    eventWorkQueued_ = false;
    ++serverStops_;
    const httpd_handle_t old = server_;
    server_ = nullptr;
    const esp_err_t stopped = httpd_stop(old);
    if (stopped != ESP_OK) serverStartError_ = int(stopped);
    if (sseSession_ && !sseSession_->valid) {
      delete sseSession_;
      sseSession_ = nullptr;
    }
  }
};
}  // namespace pp
