#pragma once
#include <WiFi.h>
#include <esp_http_server.h>
#include <esp_system.h>
#include <stdlib.h>
#include <stdarg.h>
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
#include <esp_idf_version.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
#define PP_HTTP_ASYNC_RUNTIME 1
#else
// Legacy IDF 4.x acceptance images retain their synchronous SSE fallback;
// the migrated IDF 5.x production image uses the async request API below.
#define PP_HTTP_ASYNC_RUNTIME 0
#endif
#else
#define PP_HTTP_ASYNC_RUNTIME 0
#endif
#include "noticeboard.h"
#include "system_time.h"
#include "diagnostic_trace.h"
#include "diagnostic_browser.h"
#include "diagnostic_retrieval.h"
#include "diagnostic_export.h"
#include "control_lease.h"
#include "external_wifi.h"
#ifndef PP_MAX_SSE_CLIENTS
#define PP_MAX_SSE_CLIENTS 4
#endif
#ifndef PP_HTTP_ORDINARY_REQUEST_RESERVE
#define PP_HTTP_ORDINARY_REQUEST_RESERVE 4
#endif
#ifndef PP_HTTP_TRANSPORT_DIAGNOSTICS
#define PP_HTTP_TRANSPORT_DIAGNOSTICS 0
#endif
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
                   TrackRecordStore* records = nullptr, const DetailSource* detailSource = nullptr)
      : bus_(bus), endpoint_(endpoint), noticeboard_(noticeboard),
        authority_(authority ? authority : &fallback_), history_(history), records_(records), detailSource_(detailSource) {
    controllerEpoch_ = newControllerEpoch();
  }
  uint64_t controllerEpoch() const { return controllerEpoch_; }
  // The production authority store is opened after global object construction.
  // Restore exactly once from the ready store before HTTP starts.
  bool restorePersistedAuthority() {
    if (controlLease_.hasOwner()) return true;
    uint64_t persisted = 0;
    if (!authority_->loadMaster(persisted) || !persisted) return false;
    return controlLease_.restore(persisted);
  }
  void setDiagnosticTrace(diagnostic::ControllerOperationTrace* trace) { diagnosticTrace_ = trace; }
  void setDiagnosticBuildIdentity(const char* identity) {
    diagnosticBuildIdentity_ = identity && *identity ? identity : "unknown";
  }  void setDiagnosticBuildProvenance(const char* sourceCommit, const char* environment,
                                    const char* appElfSha256) {
    diagnosticSourceCommit_ = sourceCommit && *sourceCommit ? sourceCommit : "unknown";
    diagnosticBuildEnvironment_ = environment && *environment ? environment : "unknown";
    diagnosticAppElfSha256_ = appElfSha256 && *appElfSha256 ? appElfSha256 : "unknown";
  }

  void begin() {
#if defined(PP_PHASE8B_REAL_SD)
    network_.begin();
    apStarted_ = networkApStarted();
    wifiConnected_ = apStarted_;
    lastWifiAttemptMs_ = millis();
    started_ = true;
#else
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin("Wokwi-GUEST", "", 6);
    lastWifiAttemptMs_ = millis();
    wifiConnected_ = false;
    started_ = true;
#endif
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
#if defined(PP_PHASE8B_REAL_SD)
    network_.tick();
    apStarted_ = networkApStarted();
    const bool connected = apStarted_;
#else
    const bool connected = WiFi.status() == WL_CONNECTED;
#endif
    const uint32_t now = millis();
    if (!connected) {
      wifiConnected_ = false;
      if (server_) stopServer();
      // Keep recovery bounded and non-blocking. Wokwi retries its station
      // network; the real S3 retries creation of its local P&P AP.
      if (static_cast<uint32_t>(now - lastWifiAttemptMs_) >= 5000U) {
        ++wifiReconnectAttempts_;
#if defined(PP_PHASE8B_REAL_SD)
        network_.tick();
        apStarted_ = networkApStarted();
        if (apStarted_) {
          wifiConnected_ = true;
        } else {
          Serial.printf("[WIFI] P&P AP FAILED ssid=%s\n", ExternalWifiManager::FactoryApSsid);
        }
#else
        WiFi.reconnect();
#endif
        lastWifiAttemptMs_ = now;
      }
      return;
    }
    wifiConnected_ = true;
#if defined(PP_PHASE8B_REAL_SD)
    const uint8_t clients = networkClientCount();
    if (clients != apClientCount_) {
      apClientCount_ = clients;
      Serial.printf("[WIFI] P&P AP clients=%u\n", static_cast<unsigned>(clients));
    }
#endif
    if (!server_) startServer();
    updatePresentation(systemTime());
    // Noticeboard and presentation changes are coalesced independently for
    // each bounded Browser SSE session.  A slow client cannot block another
    // client or any authoritative P&P work.
    if (server_) {
      for (size_t i = 0; i < MaxSseClients; ++i) {
        SseSession* session = sseSessions_[i];
        if (!session || !session->valid) continue;
        const bool changed = noticeRevision_ != session->sentRevision;
        const bool presentationChanged = presentationSequence_ != session->sentPresentationSequence;
        const bool keepalive = static_cast<uint32_t>(now - session->lastSendMs) >= 10000U;
        if ((changed || presentationChanged || session->controlPending || session->controlRefreshPending || keepalive) && !session->notifyPending) {
          queueEventWork(session);
        }
      }
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
      SseSession* ownerSession = findSseSessionByFingerprint(controlLease_.owner());
      if (ownerSession) {
        ownerSession->controlRequestId = requestId;
        ownerSession->controlPending = true;
        queueEventWork(ownerSession);
      }
    }
    if (result == ControlLeaseResult::Accepted || result == ControlLeaseResult::Refused) {
      queueControlRefresh();
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
    if (result == ControlLeaseResult::Accepted || result == ControlLeaseResult::Refused) {
      queueControlRefresh();
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
  bool accessPointStarted() const {
#if defined(PP_PHASE8B_REAL_SD)
    return apStarted_;
#else
    return false;
#endif
  }
  const char* networkSsid() const {
#if defined(PP_PHASE8B_REAL_SD)
    return Phase8bApSsid;
#else
    return "Wokwi-GUEST";
#endif
  }
  String networkAddress() const {
#if defined(PP_PHASE8B_REAL_SD)
    return WiFi.softAPIP().toString();
#else
    return WiFi.localIP().toString();
#endif
  }
  uint8_t connectedClientCount() const {
#if defined(PP_PHASE8B_REAL_SD)
    return networkClientCount();
#else
    return 0;
#endif
  }
  struct NetworkStatus { ExternalWifiManager::Status value; };
  void networkStatus(NetworkStatus& value) const {
#if defined(PP_PHASE8B_REAL_SD)
    network_.status(value.value);
#else
    value.value = ExternalWifiManager::Status{};
#endif
  }
  bool networkConnect(const char* ssid, const char* password) {
#if defined(PP_PHASE8B_REAL_SD)
    return network_.connect(ssid, password);
#else
    (void)ssid; (void)password; return false;
#endif
  }
  bool networkReconnect() {
#if defined(PP_PHASE8B_REAL_SD)
    return network_.reconnect();
#else
    return false;
#endif
  }
  bool networkDisconnect() {
#if defined(PP_PHASE8B_REAL_SD)
    return network_.disconnect();
#else
    return false;
#endif
  }
  bool networkForget() {
#if defined(PP_PHASE8B_REAL_SD)
    return network_.forget();
#else
    return false;
#endif
  }
  bool networkStartScan() {
#if defined(PP_PHASE8B_REAL_SD)
    return network_.startScan();
#else
    return false;
#endif
  }
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
  bool serializeResultsForAcceptance(char* out,size_t cap,size_t&length)const{auto* loaded=static_cast<RaceEngineModule::CompletedRaceResult*>(calloc(1,sizeof(RaceEngineModule::CompletedRaceResult)));if(!loaded)return false;const bool ok=serializeResults(displayResult(*loaded),out,cap,length,detailSource_);RaceEngineModule::clearResult(*loaded);free(loaded);return ok;}
  bool serializeDetailsForAcceptance(char* out,size_t cap,size_t&length)const{auto* loaded=static_cast<RaceEngineModule::CompletedRaceResult*>(calloc(1,sizeof(RaceEngineModule::CompletedRaceResult)));if(!loaded)return false;const bool ok=serializeDetails(displayResult(*loaded),out,cap,length);RaceEngineModule::clearResult(*loaded);free(loaded);return ok;}
  bool serializeRecordsForAcceptance(char* out,size_t cap,size_t&length)const{return serializeRecords(records_,current().entryCount,out,cap,length);}
  bool serializeHistoryForAcceptance(char* out,size_t cap,size_t&length)const{auto* scratch=static_cast<RaceEngineModule::CompletedRaceResult*>(calloc(1,sizeof(RaceEngineModule::CompletedRaceResult)));if(!scratch)return false;const bool ok=serializeHistory(history_,out,cap,length,scratch);RaceEngineModule::clearResult(*scratch);free(scratch);return ok;}
  static bool serializeStateForAcceptance(const NoticeboardState& value,Time now,char*out,size_t cap,size_t&length){return serializeState(value,now,out,cap,length);}
  static bool serializeResultsForAcceptance(const RaceEngineModule::CompletedRaceResult& value,char*out,size_t cap,size_t&length,const DetailSource* detailSource=nullptr){return serializeResults(value,out,cap,length,detailSource);}
  static bool serializeDetailsForAcceptance(const RaceEngineModule::CompletedRaceResult& value,char*out,size_t cap,size_t&length){return serializeDetails(value,out,cap,length);}
  static bool serializeRecordsForAcceptance(TrackRecordStore* records,uint8_t count,char*out,size_t cap,size_t&length){return serializeRecords(records,count,out,cap,length);}
private:
  static constexpr const char* Phase8bApSsid = "Plonk-and-Play";
  uint32_t contextClientTag(uint64_t owner) {
    const uint64_t value = fingerprint(owner);
    for (size_t i = 0; i < ContextClientTagCapacity; ++i) {
      if (contextClientFingerprints_[i] == value && contextClientTags_[i] != 0) {
        return contextClientTags_[i];
      }
    }
    for (size_t i = 0; i < ContextClientTagCapacity; ++i) {
      if (contextClientTags_[i] == 0) {
        uint32_t tag = nextContextClientTag_++;
        if (tag == 0) tag = nextContextClientTag_++;
        contextClientFingerprints_[i] = value;
        contextClientTags_[i] = tag;
        return tag;
      }
    }
    // Bounded diagnostic aid: never expose the Browser token itself.
    return static_cast<uint32_t>((value ^ (value >> 32)) | 0x80000000u);
  }
  void traceContextResponse(uint32_t clientTag, bool hasMaster, bool isMaster, bool ownerConnected,
                            bool takeoverPending, bool takeoverRequestedByThis,
                            ControlLeaseResult result, esp_err_t sent) {
    const bool changed = !contextTraceValid_ ||
      contextTraceClientTag_ != clientTag ||
      contextTraceHasMaster_ != hasMaster ||
      contextTraceIsMaster_ != isMaster ||
      contextTraceOwnerConnected_ != ownerConnected ||
      contextTracePending_ != takeoverPending ||
      contextTraceRequestedByThis_ != takeoverRequestedByThis ||
      contextTraceResult_ != result;
    if (sent != ESP_OK || changed) {
      Serial.printf("[AUTH] CONTEXT response client_tag=%lu has_master=%s is_master=%s owner_connected=%s takeover_pending=%s requested_by_this=%s result=%s sent=%d\n",
                    static_cast<unsigned long>(clientTag),
                    hasMaster ? "true" : "false", isMaster ? "true" : "false",
                    ownerConnected ? "true" : "false",
                    takeoverPending ? "true" : "false",
                    takeoverRequestedByThis ? "true" : "false",
                    controlResultText(result), static_cast<int>(sent));
      contextTraceValid_ = true;
      contextTraceClientTag_ = clientTag;
      contextTraceHasMaster_ = hasMaster;
      contextTraceIsMaster_ = isMaster;
      contextTraceOwnerConnected_ = ownerConnected;
      contextTracePending_ = takeoverPending;
      contextTraceRequestedByThis_ = takeoverRequestedByThis;
      contextTraceResult_ = result;
    }
  }
  bool networkApStarted() const {
#if defined(PP_PHASE8B_REAL_SD)
    ExternalWifiManager::Status status{};
    network_.status(status);
    return status.apStarted;
#else
    return false;
#endif
  }
  uint8_t networkClientCount() const {
#if defined(PP_PHASE8B_REAL_SD)
    ExternalWifiManager::Status status{};
    network_.status(status);
    return status.apClients;
#else
    return 0;
#endif
  }
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
  const DetailSource* detailSource_ = nullptr;
  VolatileAuthorityStore fallback_;
  ExternalWifiManager network_{};
  httpd_handle_t server_ = nullptr;
  uint32_t serverGeneration_ = 0;
  ControlLease controlLease_{};
  uint64_t controllerEpoch_ = 0;
  diagnostic::ControllerOperationTrace* diagnosticTrace_ = nullptr;
  const char* diagnosticBuildIdentity_ = "unknown";
  const char* diagnosticSourceCommit_ = "unknown";
  const char* diagnosticBuildEnvironment_ = "unknown";
  const char* diagnosticAppElfSha256_ = "unknown";
  uint32_t noticeRevision_ = 0;
  // Clock/start-light presentation is a separate P&P-driven stream. It does
  // not advance the Noticeboard revision or generate NOTICEBOARD_CHANGED.
  volatile uint32_t presentationSequence_ = 0;
  bool presentationKnown_ = false;
  NoticeboardPresentationState presentation_{};
  uint32_t factRevision_ = 0;
  uint32_t nextCorrelation_ = 1;
  Message lastFact_{};
  bool hasFact_ = false;
  bool started_ = false;
  bool wifiConnected_ = false;
  bool contextTraceValid_ = false;
  static constexpr size_t ContextClientTagCapacity = 16;
  uint64_t contextClientFingerprints_[ContextClientTagCapacity]{};
  uint32_t contextClientTags_[ContextClientTagCapacity]{};
  uint32_t nextContextClientTag_ = 1;
  uint32_t contextTraceClientTag_ = 0;
  bool contextTraceHasMaster_ = false;
  bool contextTraceIsMaster_ = false;
  bool contextTraceOwnerConnected_ = false;
  bool contextTracePending_ = false;
  bool contextTraceRequestedByThis_ = false;
  ControlLeaseResult contextTraceResult_ = ControlLeaseResult::NoPending;
#if defined(PP_PHASE8B_REAL_SD)
  bool apStarted_ = false;
  uint8_t apClientCount_ = 0;
#endif
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
  // Provisional hardware/development capacity. This is intentionally a
  // build-time limit, not a frozen product maximum.
  static constexpr size_t MaxSseClients = PP_MAX_SSE_CLIENTS;
  static constexpr size_t MaxHttpClientSockets = 10; // max_open_sockets(13) minus ESP-IDF's three internal sockets
  static constexpr size_t OrdinaryRequestReserve = PP_HTTP_ORDINARY_REQUEST_RESERVE;
  static constexpr size_t PageWorkerCount = 2;
  static constexpr size_t PageQueueDepth = 2;
  static constexpr uint32_t PageWorkerStackWords = 2048; // 8192 bytes per worker
  // Diagnostic starting point: the 4096-byte allocation overflowed in the
  // initial IDF response path. Final product sizing remains evidence-led.
  static constexpr uint32_t SseWorkerStackWords = 2048;  // 8192 bytes per stream
  static constexpr size_t SseInitialFrameCapacity = 128;
  static constexpr size_t SsePayloadCapacity = 192;
  static constexpr uint32_t SseStalledSendThresholdMs = 250;
  enum class SseCleanupReason : uint8_t { None=0, Disconnect=1, Replacement=2, SendFailure=3, Shutdown=4 };
  struct SseSession {
    BrowserInterface* owner = nullptr;
    httpd_handle_t server = nullptr;
    uint32_t serverGeneration = 0;
    int fd = -1;
    uint64_t clientToken = 0;
    uint32_t generation = 0;
    volatile bool valid = false;
    volatile bool inUse = false;
    volatile bool workerBusy = false;
    volatile bool transportClosed = false;
    volatile bool completionStarted = false;
    volatile bool notifyPending = false;
    volatile bool initialPending = false;
    volatile uint32_t queuedGeneration = 0;
    volatile uint32_t sentRevision = 0;
    volatile uint32_t sentPresentationSequence = 0;
    uint32_t lastSendMs = 0;
    volatile bool controlPending = false;
    volatile bool controlRefreshPending = false;
    volatile uint32_t controlRequestId = 0;
    httpd_req_t* asyncRequest = nullptr;
    // Transport frames belong to the session, not to the worker stack.
    char initialFrame[SseInitialFrameCapacity]{};
    char payloadFrame[SsePayloadCapacity]{};
    volatile SseCleanupReason cleanupReason = SseCleanupReason::None;
    uint16_t stackHwmIdle = UINT16_MAX;
    uint16_t stackHwmInitial = UINT16_MAX;
    uint16_t stackHwmNotification = UINT16_MAX;
    uint16_t stackHwmKeepalive = UINT16_MAX;
    uint16_t stackHwmStalled = UINT16_MAX;
    uint16_t stackHwmFailure = UINT16_MAX;
    uint16_t stackHwmDisconnect = UINT16_MAX;
    uint16_t stackHwmReplacement = UINT16_MAX;
    uint16_t stackHwmShutdown = UINT16_MAX;
#if PP_HTTP_ASYNC_RUNTIME
    TaskHandle_t worker = nullptr;
#endif
  };
  struct PageJob {
    BrowserInterface* owner = nullptr;
    httpd_handle_t server = nullptr;
    int fd = -1;
    httpd_req_t* asyncRequest = nullptr;
    uint32_t generation = 0;
    uint32_t serverGeneration = 0;
  };
  SseSession* sseSessions_[MaxSseClients]{};
  SseSession sseStorage_[MaxSseClients]{};
  uint32_t nextEventGeneration_ = 0;
#if PP_HTTP_ASYNC_RUNTIME
  QueueHandle_t pageQueue_ = nullptr;
  TaskHandle_t pageWorkers_[PageWorkerCount]{};
  volatile uint32_t nextPageGeneration_ = 0;
  volatile uint32_t pageJobsInFlight_ = 0;
  bool asyncWorkersStarted_ = false;
#endif
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
    int fd = -1;
    bool transportDetailed = false;
    esp_err_t result = ESP_OK;
    RequestTrace(BrowserInterface* value, const char* name, httpd_req_t* request = nullptr, bool detailed = false)
        : browser(value), route(name), started(millis()), fd(request ? httpd_req_to_sockfd(request) : -1), transportDetailed(detailed) {
      browser->requestEntered(route); browser->bootRequestEntered(route);
      if (transportDetailed) browser->transportTrace("enter", route, fd, ESP_OK, 0);
    }
    ~RequestTrace() {
      const uint32_t elapsed = static_cast<uint32_t>(millis() - started);
      browser->requestCompleted(route, result, elapsed); browser->bootRequestCompleted(route, result);
      if (transportDetailed) browser->transportTrace("exit", route, fd, result, elapsed);
    }
    void complete(esp_err_t value) { result = value; }
    void sendBegin() { if (transportDetailed) browser->transportTrace("send_begin", route, fd, ESP_OK, 0); }
    void sendComplete(esp_err_t value) { if (transportDetailed) browser->transportTrace("send_complete", route, fd, value, 0); }
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
  size_t httpClientCount() const {
    if (!server_) return 0;
    size_t capacity = MaxHttpClientSockets + 3;
    int fds[MaxHttpClientSockets + 3]{};
    return httpd_get_client_list(server_, &capacity, fds) == ESP_OK ? capacity : SIZE_MAX;
  }
  void transportTrace(const char* phase, const char* route, int fd, esp_err_t result, uint32_t elapsedMs) const {
#if PP_HTTP_TRANSPORT_DIAGNOSTICS
    const bool hasContext = server_ && fd >= 0 && httpd_sess_get_ctx(server_, fd) != nullptr;
    Serial.printf("[HTTP_TRACE] phase=%s route=%s fd=%d session_ctx=%u clients=%u active=%lu result=%d elapsed_ms=%lu server_generation=%lu\n",
                  phase, route, fd, hasContext ? 1u : 0u, static_cast<unsigned>(httpClientCount()),
                  static_cast<unsigned long>(httpActiveHandlers_), static_cast<int>(result),
                  static_cast<unsigned long>(elapsedMs), static_cast<unsigned long>(serverGeneration_));
#else
    (void)phase; (void)route; (void)fd; (void)result; (void)elapsedMs;
#endif
  }
  void transportAdmissionTrace(const char* route, int fd, size_t clients, size_t sseSlots,
                               size_t pageSpaces, const char* outcome) const {
#if PP_HTTP_TRANSPORT_DIAGNOSTICS
    Serial.printf("[HTTP_ADMISSION] route=%s fd=%d outcome=%s clients=%u sse_slots=%u page_spaces=%u server_generation=%lu\n",
                  route, fd, outcome, static_cast<unsigned>(clients), static_cast<unsigned>(sseSlots),
                  static_cast<unsigned>(pageSpaces), static_cast<unsigned long>(serverGeneration_));
#else
    (void)route; (void)fd; (void)clients; (void)sseSlots; (void)pageSpaces; (void)outcome;
#endif
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
      const auto state=noticeboard_.current();
      if(state.lifecycle!=SessionLifecycle::Finished){RaceEngineModule::clearResult(loaded);return loaded;}
      // Once the current result has been committed, serve that committed
      // History payload.  The sealed engine object is still needed as the
      // provisional result while persistence is pending, but streamed detail
      // is finalized by then and is no longer a safe source for an HTTP
      // Results request.  Returning the current object after HistoryStored
      // therefore loses finish durations and other reconstructed fields.
      if(!state.persistencePending&&!state.persistenceFault&&state.historySequence&&history_){
        size_t bytes=0; uint32_t stored=0;
        if(history_->loadSequence(state.historySequence,reinterpret_cast<uint8_t*>(&loaded),sizeof(loaded),bytes,stored)&&bytes==sizeof(loaded)&&stored==state.historySequence&&loaded.sealed&&loaded.formatVersion==RaceEngineModule::ResultFormatVersion){found=true;return loaded;}
      }
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
      case SessionOperation::PracticeAgain: return diagnostic::OperationName::PracticeAgain;
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
      case RequestRejection::LifecycleNotPracticeAgain: return "PRACTICE AGAIN is not valid in the current lifecycle state";
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
    static volatile uint32_t nextHeader = 0;
    const uint32_t headerIndex = __atomic_fetch_add(&nextHeader, 1U, __ATOMIC_RELAXED) % 16U;
    char* header = headers[headerIndex];
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
  // Presentation-only elapsed finish duration.  completionTime remains the
  // raw monotonic Relevant-Time value used for authoritative ordering and
  // persistence.  The API supplies the elapsed value so Browser code never
  // subtracts controller timestamps.
  static Time finishDurationUs(const RaceEngineModule::ResultEntry& entry,const DetailSource* detailSource=nullptr){
    // TIME is the elapsed race duration at this entry's own permitted final
    // crossing.  A racer stopped by an immediate finish has no final crossing
    // and therefore has no TIME unless it completed one or more laps; in that
    // case the last completed lap's elapsed race time is authoritative.
    if(!entry.recordCount)return 0;
    Time origin=0,lastRacing=0,finalRacing=0; bool haveOrigin=false;
    auto consume=[&](const LapRecord& lap){
      if(!lap.valid)return;
      if(!haveOrigin||lap.startTime<origin){origin=lap.startTime;haveOrigin=true;}
      const Time racing=lap.racingFinishTime?lap.racingFinishTime:lap.finishTime;
      if(racing>lastRacing)lastRacing=racing;
      if(entry.completed&&entry.completionTime&&lap.finishTime==entry.completionTime)finalRacing=racing;
    };
    if(entry.records){
      for(uint32_t i=0;i<entry.recordCount;++i)consume(entry.records[i]);
    } else if(detailSource){
      uint8_t sourceEntry=detailSource->detailEntryCount();
      for(uint8_t i=0;i<sourceEntry;++i){
        if(detailSource->detailEntryId(i)==entry.raceEntryId){sourceEntry=i;break;}
      }
      if(sourceEntry>=detailSource->detailEntryCount())return 0;
      const uint32_t available=detailSource->detailRecordCount(sourceEntry);
      const uint32_t count=available<entry.recordCount?available:entry.recordCount;
      for(uint32_t i=0;i<count;++i){LapRecord lap{};if(!detailSource->readDetail(sourceEntry,i,lap))return 0;consume(lap);}
    }
    if(!haveOrigin||!lastRacing)return 0;
    const Time finish=finalRacing?finalRacing:lastRacing;
    return finish>=origin?finish-origin:0;
  }
  static bool serializeResults(const RaceEngineModule::CompletedRaceResult&r,char*out,size_t cap,size_t&length,const DetailSource* detailSource=nullptr){
    const uint64_t overtimeUs=r.overtime&&r.finishTime>r.expiryTime?r.finishTime-r.expiryTime:0;size_t n=0;const char* mode=r.mode==SessionMode::Endurance?"ENDURANCE":r.mode==SessionMode::OpenPractice?"OPEN_PRACTICE":"LAP_RACE";bool ok=true;ok=appendJson(out,cap,n,"{\"sealed\":%s,\"valid\":%s,\"mode\":\"%s\"",r.sealed?"true":"false",r.valid?"true":"false",mode)&&ok;ok=appendJson(out,cap,n,",\"lapTarget\":%lu,\"durationMinutes\":%u,\"durationUs\":%llu",(unsigned long)r.lapTarget,unsigned(r.durationMinutes),(unsigned long long)r.durationUs)&&ok;ok=appendJson(out,cap,n,",\"expiryTime\":%llu,\"overtime\":%s,\"overtimeUs\":%llu",(unsigned long long)r.expiryTime,r.overtime?"true":"false",(unsigned long long)overtimeUs)&&ok;ok=appendJson(out,cap,n,",\"winningTime\":%llu,\"finishTime\":%llu,\"finishBehaviour\":\"%s\"",(unsigned long long)r.winningTime,(unsigned long long)r.finishTime,finishBehaviour(r.behaviour))&&ok;ok=appendJson(out,cap,n,",\"deadHeat\":%s,\"fastestLap\":%llu,\"fastestEntryId\":%lu,\"fastestLapTied\":%s,\"entries\":[",r.deadHeat?"true":"false",(unsigned long long)r.fastestLap,(unsigned long)r.fastestEntryId,r.fastestLapTied?"true":"false")&&ok;
    if(r.entryCount>PP_MAX_ENTRIES)ok=false;const uint8_t count=r.entryCount>PP_MAX_ENTRIES?PP_MAX_ENTRIES:r.entryCount;
    for(uint8_t i=0;i<count;++i){const auto&e=r.entries[i];ok=appendJson(out,cap,n,"%s{\"raceEntryId\":%lu,\"mugId\":%lu,\"lane\":%u,\"laps\":%lu,\"classifiedLaps\":%lu,\"lapPenalty\":%lu,\"rank\":%lu,\"tied\":%s,\"lapsBehind\":%lu,\"completed\":%s,\"completionTime\":%llu,\"finishDurationUs\":%llu,\"bestLap\":%llu}",i?",":"",(unsigned long)e.raceEntryId,(unsigned long)e.mugId,unsigned(e.lane),(unsigned long)e.laps,(unsigned long)e.classifiedLaps,(unsigned long)e.lapPenalty,(unsigned long)e.rank,e.tied?"true":"false",(unsigned long)e.lapsBehind,e.completed?"true":"false",(unsigned long long)e.completionTime,(unsigned long long)finishDurationUs(e,detailSource),(unsigned long long)e.bestLap)&&ok;}
    ok=appendJson(out,cap,n,"]}")&&ok;length=n;return ok;
  }
  static bool serializeDetails(const RaceEngineModule::CompletedRaceResult&r,char*out,size_t cap,size_t&length){
    // Streamed detail may legitimately leave the resident record pointer
    // empty while retaining the authoritative record count.  Details has no
    // DetailSource boundary, so fail closed before emitting any response
    // rather than dereferencing absent storage.
    for(uint8_t i=0;i<r.entryCount&&i<PP_MAX_ENTRIES;++i){
      const auto&e=r.entries[i];
      if(e.recordCount&&e.records==nullptr){length=0;return false;}
    }
    size_t n=0;bool ok=appendJson(out,cap,n,"{\"sealed\":%s,\"lapTarget\":%lu,\"entries\":[",r.sealed?"true":"false",(unsigned long)r.lapTarget);
    if(r.entryCount>PP_MAX_ENTRIES)ok=false;const uint8_t count=r.entryCount>PP_MAX_ENTRIES?PP_MAX_ENTRIES:r.entryCount;
    for(uint8_t i=0;i<count;++i){const auto&e=r.entries[i];ok=appendJson(out,cap,n,"%s{\"raceEntryId\":%lu,\"mugId\":%lu,\"lane\":%u,\"laps\":%lu,\"rank\":%lu,\"records\":[",i?",":"",(unsigned long)e.raceEntryId,(unsigned long)e.mugId,unsigned(e.lane),(unsigned long)e.laps,(unsigned long)e.rank)&&ok;for(uint32_t j=0;j<e.recordCount;++j){const auto&lap=e.records[j];ok=appendJson(out,cap,n,"%s{\"lapNumber\":%lu,\"startTime\":%llu,\"finishTime\":%llu,\"lapTime\":%llu,\"valid\":%s}",j?",":"",(unsigned long)lap.lapNumber,(unsigned long long)lap.startTime,(unsigned long long)lap.finishTime,(unsigned long long)lap.lapTime,lap.valid?"true":"false")&&ok;}ok=appendJson(out,cap,n,"]}")&&ok;}
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
    auto* loaded=scratch?scratch:static_cast<RaceEngineModule::CompletedRaceResult*>(calloc(1,sizeof(RaceEngineModule::CompletedRaceResult)));const bool owned=!scratch;
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
    ok=appendJson(out,cap,n,"]}")&&ok;if(owned)RaceEngineModule::clearResult(*loaded);if(owned)free(loaded);return ok;
  }
  static esp_err_t state(httpd_req_t* request) {
    RequestTrace trace(instance(), "/state", request, true);
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
    trace.sendBegin();
    const esp_err_t sent=httpd_resp_sendstr(request,json);
    trace.sendComplete(sent);
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
    auto* loaded=static_cast<RaceEngineModule::CompletedRaceResult*>(calloc(1,sizeof(RaceEngineModule::CompletedRaceResult))); char* json=static_cast<char*>(malloc(ResultsJsonCapacity));
    if(!loaded||!json){if(loaded)RaceEngineModule::clearResult(*loaded);free(loaded);free(json);httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"result buffer unavailable");trace.complete(ESP_ERR_NO_MEM);return ESP_FAIL;}
    bool found=false; const auto&r=instance()->displayResult(*loaded,sequence,found); if(sequence&&!found){RaceEngineModule::clearResult(*loaded);free(loaded);free(json);httpd_resp_send_err(request,HTTPD_404_NOT_FOUND,"history result not found");trace.complete(ESP_ERR_NOT_FOUND);return ESP_FAIL;}
    // A committed History result has already reconstructed its own lap detail.
    // Do not let the live Race Engine detail source shadow that payload: the
    // active source may be finalized/reset by the time the HTTP request runs,
    // which would preserve progress/BEST metadata but erase per-entry TIME.
    size_t length=0; const DetailSource* detailSource=(sequence||found)?nullptr:instance()->detailSource_; if(!serializeResults(r,json,ResultsJsonCapacity,length,detailSource)){RaceEngineModule::clearResult(*loaded);free(loaded);free(json);httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"result entry count exceeds capacity");trace.complete(ESP_ERR_INVALID_SIZE);return ESP_FAIL;}
    httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");const esp_err_t sent=httpd_resp_sendstr(request,json);RaceEngineModule::clearResult(*loaded);free(loaded);free(json);trace.complete(sent);return sent;
  }
  static esp_err_t detailsRoute(httpd_req_t* request) {
    RequestTrace trace(instance(), "/details"); client(request); uint32_t sequence=0;
    if(!historySequenceQuery(request,sequence)){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"invalid history sequence");trace.complete(ESP_ERR_INVALID_ARG);return ESP_FAIL;}
    auto* loaded=static_cast<RaceEngineModule::CompletedRaceResult*>(calloc(1,sizeof(RaceEngineModule::CompletedRaceResult))); char* chunk=static_cast<char*>(malloc(320));
    if(!loaded||!chunk){if(loaded)RaceEngineModule::clearResult(*loaded);free(loaded);free(chunk);httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"details buffer unavailable");trace.complete(ESP_ERR_NO_MEM);return ESP_FAIL;}
    bool found=false; const auto&r=instance()->displayResult(*loaded,sequence,found); if(sequence&&!found){RaceEngineModule::clearResult(*loaded);free(loaded);free(chunk);httpd_resp_send_err(request,HTTPD_404_NOT_FOUND,"history result not found");trace.complete(ESP_ERR_NOT_FOUND);return ESP_FAIL;}
    // A streamed/degraded result can retain recordCount without resident
    // records.  This route has no detail source to read from, so report the
    // unavailable detail explicitly and never enter the record loop.
    for(uint8_t i=0;i<r.entryCount&&i<PP_MAX_ENTRIES;++i){
      const auto&e=r.entries[i];
      if(e.recordCount&&e.records==nullptr){
        RaceEngineModule::clearResult(*loaded);free(loaded);free(chunk);
        httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"detail unavailable");
        trace.complete(ESP_ERR_INVALID_STATE);return ESP_FAIL;
      }
    }
    httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");snprintf(chunk,320,"{\"sealed\":%s,\"lapTarget\":%lu,\"entries\":[",r.sealed?"true":"false",(unsigned long)r.lapTarget);httpd_resp_send_chunk(request,chunk,HTTPD_RESP_USE_STRLEN);
    for(uint8_t i=0;i<r.entryCount;++i){const auto&e=r.entries[i];snprintf(chunk,320,"%s{\"raceEntryId\":%lu,\"mugId\":%lu,\"lane\":%u,\"laps\":%lu,\"rank\":%lu,\"records\":[",i?",":"",(unsigned long)e.raceEntryId,(unsigned long)e.mugId,unsigned(e.lane),(unsigned long)e.laps,(unsigned long)e.rank);httpd_resp_send_chunk(request,chunk,HTTPD_RESP_USE_STRLEN);for(uint32_t j=0;j<e.recordCount;++j){const auto&lap=e.records[j];snprintf(chunk,320,"%s{\"lapNumber\":%lu,\"startTime\":%llu,\"finishTime\":%llu,\"lapTime\":%llu,\"valid\":%s}",j?",":"",(unsigned long)lap.lapNumber,(unsigned long long)lap.startTime,(unsigned long long)lap.finishTime,(unsigned long long)lap.lapTime,lap.valid?"true":"false");httpd_resp_send_chunk(request,chunk,HTTPD_RESP_USE_STRLEN);}httpd_resp_send_chunk(request,"]}",2);}
    httpd_resp_send_chunk(request,"]}",2);const esp_err_t sent=httpd_resp_send_chunk(request,nullptr,0);RaceEngineModule::clearResult(*loaded);free(loaded);free(chunk);trace.complete(sent);return sent;
  }
  static esp_err_t historyRoute(httpd_req_t* request) {
    RequestTrace trace(instance(), "/history"); client(request); BrowserInterface* browser=instance();
    auto* stored=static_cast<RaceEngineModule::CompletedRaceResult*>(calloc(1,sizeof(RaceEngineModule::CompletedRaceResult)));
    if(!stored){httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"history buffer unavailable");trace.complete(ESP_ERR_NO_MEM);return ESP_FAIL;}
    httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");
    esp_err_t sent=httpd_resp_send_chunk(request,"{\"entries\":[",HTTPD_RESP_USE_STRLEN);
    const uint8_t total=browser->history_?browser->history_->count():0;
    for(uint8_t i=0;i<total&&sent==ESP_OK;++i){
      size_t bytes=0;uint32_t sequence=0;
      if(!browser->history_->loadNewest(i,reinterpret_cast<uint8_t*>(stored),sizeof(*stored),bytes,sequence)||bytes!=sizeof(*stored)||!stored->sealed||stored->formatVersion!=RaceEngineModule::ResultFormatVersion){sent=ESP_ERR_INVALID_SIZE;break;}
      char chunk[1536];const char* mode=stored->mode==SessionMode::Endurance?"ENDURANCE":stored->mode==SessionMode::OpenPractice?"OPEN_PRACTICE":"LAP_RACE";
      const uint64_t overtimeUs=stored->overtime&&stored->finishTime>stored->expiryTime?stored->finishTime-stored->expiryTime:0;
      const int n=snprintf(chunk,sizeof(chunk),"%s{\"sequence\":%lu,\"sealed\":true,\"mode\":\"%s\",\"durationMinutes\":%u,\"lapTarget\":%lu,\"finishBehaviour\":\"%s\",\"winningTime\":%llu,\"finishTime\":%llu,\"expiryTime\":%llu,\"overtime\":%s,\"overtimeUs\":%llu,\"fastestLap\":%llu,\"entryCount\":%u,\"entries\":[",i?",":"",(unsigned long)sequence,mode,unsigned(stored->durationMinutes),(unsigned long)stored->lapTarget,finishBehaviour(stored->behaviour),(unsigned long long)stored->winningTime,(unsigned long long)stored->finishTime,(unsigned long long)stored->expiryTime,stored->overtime?"true":"false",(unsigned long long)overtimeUs,(unsigned long long)stored->fastestLap,unsigned(stored->entryCount));
      if(n<0||static_cast<size_t>(n)>=sizeof(chunk)){sent=ESP_ERR_INVALID_SIZE;break;}
      sent=httpd_resp_send_chunk(request,chunk,n);
      for(uint8_t j=0;j<stored->entryCount&&sent==ESP_OK;++j){const auto&e=stored->entries[j];const int m=snprintf(chunk,sizeof(chunk),"%s{\"raceEntryId\":%lu,\"mugId\":%lu,\"lane\":%u,\"laps\":%lu,\"classifiedLaps\":%lu,\"lapPenalty\":%lu,\"rank\":%lu,\"lapsBehind\":%lu,\"completed\":%s}",j?",":"",(unsigned long)e.raceEntryId,(unsigned long)e.mugId,unsigned(e.lane),(unsigned long)e.laps,(unsigned long)e.classifiedLaps,(unsigned long)e.lapPenalty,(unsigned long)e.rank,(unsigned long)e.lapsBehind,e.completed?"true":"false");if(m<0||static_cast<size_t>(m)>=sizeof(chunk)){sent=ESP_ERR_INVALID_SIZE;break;}sent=httpd_resp_send_chunk(request,chunk,m);}
      if(sent==ESP_OK)sent=httpd_resp_send_chunk(request,"]}",2);
    }
    if(sent==ESP_OK)sent=httpd_resp_send_chunk(request,"]}",2);
    if(sent==ESP_OK)sent=httpd_resp_send_chunk(request,nullptr,0);
    RaceEngineModule::clearResult(*stored);free(stored);trace.complete(sent);return sent;
  }
  static esp_err_t recordsRoute(httpd_req_t* request) {
    client(request); BrowserInterface* browser=instance();
    char* json=static_cast<char*>(malloc(RecordsJsonCapacity));size_t length=0;
    if(!json){httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"records buffer unavailable");return ESP_FAIL;}
    if(!serializeRecords(browser->records_,browser->noticeboard_.current().entryCount,json,RecordsJsonCapacity,length)){free(json);httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"record entry count exceeds capacity");return ESP_FAIL;}
    httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");const esp_err_t sent=httpd_resp_sendstr(request,json);free(json);return sent;
  }
  static const char* sseCleanupText(SseCleanupReason reason) {
    switch (reason) {
      case SseCleanupReason::Disconnect: return "disconnect";
      case SseCleanupReason::Replacement: return "replacement";
      case SseCleanupReason::SendFailure: return "send_failure";
      case SseCleanupReason::Shutdown: return "shutdown";
      default: return "none";
    }
  }
  void recordSseStack(SseSession* session, const char* stage, volatile uint16_t& lowWater) {
#if PP_HTTP_TRANSPORT_DIAGNOSTICS
    if (!session) return;
    const UBaseType_t words = uxTaskGetStackHighWaterMark(nullptr);
    const uint16_t measured = words > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(words);
    if (measured < lowWater) lowWater = measured;
    Serial.printf("[SSE_STACK] generation=%lu fd=%d stage=%s hwm_words=%u stack_words=%lu cleanup=%s\n",
                  static_cast<unsigned long>(session->generation), session->fd, stage,
                  static_cast<unsigned>(measured), static_cast<unsigned long>(SseWorkerStackWords),
                  sseCleanupText(session->cleanupReason));
#else
    (void)session; (void)stage; (void)lowWater;
#endif
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
    session->transportClosed = true;
    BrowserInterface* browser = session->owner;
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
    Serial.printf("[SSE] disconnected generation=%lu fd=%d at_us=%llu\n",
      static_cast<unsigned long>(session->generation), session->fd,
      static_cast<unsigned long long>(systemTime()));
#endif
    if (browser) browser->onSseTransportClosed(session);
  }
#if PP_HTTP_ASYNC_RUNTIME
  static void pageWorkerTask(void* argument) {
    auto* browser = static_cast<BrowserInterface*>(argument);
    if (!browser) vTaskDelete(nullptr);
    PageJob job{};
    for (;;) {
      if (xQueueReceive(browser->pageQueue_, &job, portMAX_DELAY) != pdTRUE) continue;
      if (!job.asyncRequest) continue;
      browser->runPageJob(job);
    }
  }
  static void sseWorkerTask(void* argument) {
    auto* session = static_cast<SseSession*>(argument);
    if (!session || !session->owner) vTaskDelete(nullptr);
    session->owner->recordSseStack(session, "idle", session->stackHwmIdle);
    for (;;) {
      (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      const uint32_t workGeneration = session->queuedGeneration;
      if (session->owner && workGeneration) session->owner->runSseWorker(session, workGeneration);
    }
  }
#endif
  void onSseTransportClosed(SseSession* session) {
    if (!session) return;
    session->valid = false;
    session->transportClosed = true;
    session->cleanupReason = SseCleanupReason::Disconnect;
    if (findSseSessionExact(session) == session) {
      removeSseSession(session);
      controlDisconnected(session->clientToken);
    }
#if PP_HTTP_ASYNC_RUNTIME
    if (session->worker) xTaskNotifyGive(session->worker);
#else
    runSseWorker(session, session->generation);
#endif
  }
  void completeAsyncRequest(httpd_req_t* request) {
#if PP_HTTP_ASYNC_RUNTIME
    if (!request) return;
    const esp_err_t result = httpd_req_async_handler_complete(request);
    if (result != ESP_OK) {
      Serial.printf("[HTTP] async_complete_failed result=%d\n", static_cast<int>(result));
    }
#else
    (void)request;
#endif
  }
  void completeSseRequest(SseSession* session) {
    if (!session || __atomic_exchange_n(&session->completionStarted, true, __ATOMIC_ACQ_REL)) return;
    const httpd_handle_t server = session->server;
    const int fd = session->fd;
    httpd_req_t* request = session->asyncRequest;
    session->asyncRequest = nullptr;
#if PP_HTTP_ASYNC_RUNTIME
    const bool serverAlive = server && server == server_ &&
                             session->serverGeneration == serverGeneration_;
    // Detach the generation-bound callback before releasing the async request;
    // otherwise a recycled fd could later invoke this session's cleanup path.
    if (serverAlive && fd >= 0 && httpd_sess_get_ctx(server, fd) == session) {
      httpd_sess_set_ctx(server, fd, nullptr, nullptr);
    }
    if (serverAlive) completeAsyncRequest(request);
#else
    (void)request;
#endif
    session->inUse = false;
    session->workerBusy = false;
    session->notifyPending = false;
    session->initialPending = false;
    session->queuedGeneration = 0;
    session->valid = false;
    session->transportClosed = false;
    __atomic_store_n(&session->completionStarted, false, __ATOMIC_RELEASE);
    session->server = nullptr;
    session->serverGeneration = 0;
    session->fd = -1;
    session->clientToken = 0;
    session->generation = 0;
    session->sentRevision = 0;
    session->sentPresentationSequence = 0;
    session->controlPending = false;
    session->controlRefreshPending = false;
    session->controlRequestId = 0;
  }
  void runSseWorker(SseSession* session, uint32_t workGeneration) {
    if (!session) return;
    if (!session->inUse || session->generation != workGeneration) {
      if (session->cleanupReason == SseCleanupReason::Replacement) {
        recordSseStack(session, "replacement", session->stackHwmReplacement);
      } else if (session->cleanupReason == SseCleanupReason::SendFailure) {
        recordSseStack(session, "send_failure", session->stackHwmFailure);
      } else if (session->cleanupReason == SseCleanupReason::Shutdown) {
        recordSseStack(session, "shutdown", session->stackHwmShutdown);
      } else {
        recordSseStack(session, "disconnect", session->stackHwmDisconnect);
      }
      return;
    }
    session->workerBusy = true;
    while (session->inUse && session->valid && !session->transportClosed) {
      session->notifyPending = false;
      sendEventNotification(session, workGeneration);
      if (!session->valid || session->transportClosed ||
          session->generation != workGeneration || !session->notifyPending) break;
    }
    if (session->inUse && (!session->valid || session->transportClosed ||
                           session->server != server_ ||
                           session->serverGeneration != serverGeneration_ ||
                           session->generation != workGeneration ||
                           !session->asyncRequest ||
                           findSseSessionExact(session) != session)) {
      if (session->cleanupReason == SseCleanupReason::Replacement) {
        recordSseStack(session, "replacement", session->stackHwmReplacement);
      } else if (session->cleanupReason == SseCleanupReason::SendFailure) {
        recordSseStack(session, "send_failure", session->stackHwmFailure);
      } else if (session->cleanupReason == SseCleanupReason::Shutdown) {
        recordSseStack(session, "shutdown", session->stackHwmShutdown);
      } else {
        recordSseStack(session, "disconnect", session->stackHwmDisconnect);
      }
      session->valid = false;
      session->transportClosed = true;
      completeSseRequest(session);
    }
    session->workerBusy = false;
  }
  void startSseWorker(SseSession* session) {
#if PP_HTTP_ASYNC_RUNTIME
    if (!session || session->worker) return;
    char name[16];
    snprintf(name, sizeof(name), "pp_sse_%u", static_cast<unsigned>(session - sseStorage_));
    if (xTaskCreate(&BrowserInterface::sseWorkerTask, name, SseWorkerStackWords,
                    session, 1, &session->worker) != pdPASS) {
      session->worker = nullptr;
    }
#else
    (void)session;
#endif
  }
  bool startHttpWorkers() {
#if PP_HTTP_ASYNC_RUNTIME
    if (asyncWorkersStarted_) return true;
    pageQueue_ = xQueueCreate(PageQueueDepth, sizeof(PageJob));
    if (!pageQueue_) return false;
    size_t pageStarted = 0;
    for (size_t i = 0; i < PageWorkerCount; ++i) {
      char name[16];
      snprintf(name, sizeof(name), "pp_page_%u", static_cast<unsigned>(i));
      if (xTaskCreate(&BrowserInterface::pageWorkerTask, name, PageWorkerStackWords,
                      this, 1, &pageWorkers_[i]) != pdPASS) {
        for (size_t j = 0; j < pageStarted; ++j) vTaskDelete(pageWorkers_[j]);
        for (size_t j = 0; j < MaxSseClients; ++j) sseStorage_[j].worker = nullptr;
        vQueueDelete(pageQueue_);
        pageQueue_ = nullptr;
        return false;
      }
      ++pageStarted;
    }
    size_t sseStarted = 0;
    for (size_t i = 0; i < MaxSseClients; ++i) {
      sseStorage_[i].owner = this;
      startSseWorker(&sseStorage_[i]);
      if (!sseStorage_[i].worker) {
        for (size_t j = 0; j < sseStarted; ++j) vTaskDelete(sseStorage_[j].worker);
        for (size_t j = 0; j < pageStarted; ++j) vTaskDelete(pageWorkers_[j]);
        for (size_t j = 0; j < MaxSseClients; ++j) sseStorage_[j].worker = nullptr;
        vQueueDelete(pageQueue_);
        pageQueue_ = nullptr;
        return false;
      }
      ++sseStarted;
    }
    asyncWorkersStarted_ = true;
    return true;
#else
    return true;
#endif
  }
  void runPageJob(PageJob& job) {
#if PP_HTTP_ASYNC_RUNTIME
    if (!job.asyncRequest) return;
    httpd_req_t* request = job.asyncRequest;
    const bool current = job.owner == this && job.server == server_ &&
                         job.serverGeneration == serverGeneration_ &&
                         httpd_req_to_sockfd(request) == job.fd;
    if (!current) {
      if (job.server && job.server == server_ &&
          job.serverGeneration == serverGeneration_) completeAsyncRequest(request);
      return;
    }
    // sendNormalPage owns the request trace; keeping it in the worker avoids
    // double-counting a page while preserving the existing route diagnostics.
    (void)sendNormalPage(request);
    completeAsyncRequest(request);
#else
    (void)job;
#endif
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

  SseSession* findSseSession(uint64_t token) const {
    for (size_t i = 0; i < MaxSseClients; ++i) {
      SseSession* session = sseSessions_[i];
      if (session && session->valid && session->clientToken == token) return session;
    }
    return nullptr;
  }
  SseSession* findSseSessionExact(SseSession* target) const {
    if (!target) return nullptr;
    for (size_t i = 0; i < MaxSseClients; ++i) {
      if (sseSessions_[i] == target) return target;
    }
    return nullptr;
  }
  SseSession* findSseSessionByFingerprint(uint64_t fingerprintValue) const {
    for (size_t i = 0; i < MaxSseClients; ++i) {
      SseSession* session = sseSessions_[i];
      if (session && session->valid && fingerprint(session->clientToken) == fingerprintValue) return session;
    }
    return nullptr;
  }
  void removeSseSession(SseSession* session) {
    for (size_t i = 0; i < MaxSseClients; ++i) {
      if (sseSessions_[i] == session) { sseSessions_[i] = nullptr; return; }
    }
  }
  SseSession* allocateSseSession(uint64_t token) {
    for (size_t i = 0; i < MaxSseClients; ++i) {
      SseSession* session = &sseStorage_[i];
      if (!session->inUse && !session->workerBusy && !session->asyncRequest) {
        session->owner = this;
        session->server = server_;
        session->serverGeneration = serverGeneration_;
        session->clientToken = token;
        session->generation = ++nextEventGeneration_;
        if (!session->generation) session->generation = ++nextEventGeneration_;
        session->valid = true;
        session->inUse = true;
        session->transportClosed = false;
        session->cleanupReason = SseCleanupReason::None;
        session->stackHwmIdle = UINT16_MAX;
        session->stackHwmInitial = UINT16_MAX;
        session->stackHwmNotification = UINT16_MAX;
        session->stackHwmKeepalive = UINT16_MAX;
        session->stackHwmStalled = UINT16_MAX;
        session->stackHwmFailure = UINT16_MAX;
        session->stackHwmDisconnect = UINT16_MAX;
        session->stackHwmReplacement = UINT16_MAX;
        session->stackHwmShutdown = UINT16_MAX;
        session->completionStarted = false;
        session->notifyPending = false;
        session->queuedGeneration = 0;
        sseSessions_[i] = session;
        return session;
      }
    }
    return nullptr;
  }
  void invalidateSseSession(SseSession* session, bool closeSocket, bool disconnectOwner = true,
                            SseCleanupReason reason = SseCleanupReason::SendFailure) {
    if (!session) return;
    if (findSseSessionExact(session) == session) {
      removeSseSession(session);
      if (disconnectOwner) controlDisconnected(session->clientToken);
    }
    session->valid = false;
    session->transportClosed = true;
    session->cleanupReason = reason;
    if (closeSocket && session->server && session->fd >= 0) {
      const esp_err_t closeResult = httpd_sess_trigger_close(session->server, session->fd);
      if (closeResult != ESP_OK && closeResult != ESP_ERR_NOT_FOUND) {
        Serial.printf("[SSE] close_failed generation=%lu fd=%d result=%d\n",
                      static_cast<unsigned long>(session->generation), session->fd,
                      static_cast<int>(closeResult));
      }
    }
    session->notifyPending = true;
#if PP_HTTP_ASYNC_RUNTIME
    if (session->worker) xTaskNotifyGive(session->worker);
#else
    runSseWorker(session, session->generation);
#endif
  }
  void queueEventWork(SseSession* session) {
    if (!server_ || !session || !session->inUse || !session->valid ||
        session->server != server_ || session->notifyPending) return;
    session->notifyPending = true;
    session->queuedGeneration = session->generation;
#if PP_HTTP_ASYNC_RUNTIME
    if (session->worker) xTaskNotifyGive(session->worker);
#else
    runSseWorker(session, session->generation);
#endif
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
    const bool lap = lapNoticeRevision_ == noticeRevision_;
    Serial.printf("[SSE] queued generation=%lu revision=%lu lap_entry=%lu lap=%lu event_id=%lu notify_us=%llu\n",
      static_cast<unsigned long>(session->generation),
      static_cast<unsigned long>(noticeRevision_),
      static_cast<unsigned long>(lap ? lapNoticeEntry_ : 0),
      static_cast<unsigned long>(lap ? lapNoticeNumber_ : 0),
      static_cast<unsigned long>(lap ? lapNoticeEventId_ : 0),
      static_cast<unsigned long long>(systemTime()));
#endif
  }
  void queueControlRefresh() {
    for (size_t i = 0; i < MaxSseClients; ++i) {
      SseSession* session = sseSessions_[i];
      if (!session || !session->valid) continue;
      session->controlRefreshPending = true;
      queueEventWork(session);
    }
  }
  void sendEventNotification(SseSession* session, uint32_t workGeneration) {
    if (!session || !session->inUse || !session->valid || !session->asyncRequest ||
        session->server != server_ || session->serverGeneration != serverGeneration_ ||
        session->generation != workGeneration ||
        findSseSessionExact(session) != session) {
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
      Serial.printf("[SSE] stale_work generation=%lu client=%llu at_us=%llu\n",
        static_cast<unsigned long>(workGeneration),
        static_cast<unsigned long long>(session ? session->clientToken : 0),
        static_cast<unsigned long long>(systemTime()));
#endif
      return;
    }
    const httpd_handle_t handle = session->server;
    const int fd = session->fd;
    if (httpd_sess_get_ctx(handle, fd) != session) {
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
      Serial.printf("[SSE] session_mismatch generation=%lu fd=%d at_us=%llu\n",
        static_cast<unsigned long>(session->generation), fd,
        static_cast<unsigned long long>(systemTime()));
#endif
      invalidateSseSession(session, true, true, SseCleanupReason::Replacement);
      return;
    }
    if (session->initialPending) {
      recordSseStack(session, "initial", session->stackHwmInitial);
      const int initialLength = snprintf(session->initialFrame, sizeof(session->initialFrame),
        "retry: 2000\ndata: {\"revision\":%lu}\n\n",
        static_cast<unsigned long>(noticeRevision_));
      const uint32_t sendStarted = millis();
      const esp_err_t initialResult = initialLength > 0
        ? httpd_resp_send_chunk(session->asyncRequest, session->initialFrame, size_t(initialLength))
        : ESP_FAIL;
      const uint32_t sendElapsed = static_cast<uint32_t>(millis() - sendStarted);
      recordSseStack(session, "initial", session->stackHwmInitial);
      if (sendElapsed >= SseStalledSendThresholdMs) recordSseStack(session, "stalled_send", session->stackHwmStalled);
      if (initialResult != ESP_OK) {
        recordSseStack(session, "send_failure", session->stackHwmFailure);
        invalidateSseSession(session, true);
      } else {
        session->initialPending = false;
        session->sentRevision = noticeRevision_;
        session->sentPresentationSequence = presentationSequence_;
        session->lastSendMs = millis();
      }
      return;
    }
    const uint32_t revision = noticeRevision_;
    const bool noticePending = revision != session->sentRevision;
    const uint32_t presentationSequence = presentationSequence_;
    const bool presentationPending = presentationSequence != session->sentPresentationSequence;
    const bool controlPending = session->controlPending;
    const bool controlRefreshPending = session->controlRefreshPending;
    const uint32_t controlRequestId = session->controlRequestId;
    const bool keepalive = !noticePending && !presentationPending && !controlPending && !controlRefreshPending;
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
    volatile uint16_t& frameHwm = keepalive ? session->stackHwmKeepalive : session->stackHwmNotification;
    recordSseStack(session, keepalive ? "keepalive" : "notification", frameHwm);
    if (keepalive) {
      // Keep the existing ten-second idle cadence, but make the liveness
      // frame visible to EventSource clients. SSE comment frames are not
      // delivered to Browser JavaScript and cannot drive its watchdog.
      length = snprintf(session->payloadFrame, sizeof(session->payloadFrame),
                        "event: heartbeat\ndata: {\"serverUs\":%llu}\n\n",
                        static_cast<unsigned long long>(systemTime()));
    } else if (noticePending) {
      length = snprintf(session->payloadFrame, sizeof(session->payloadFrame), "data: {\"revision\":%lu,\"serverUs\":%llu,\"lapEntry\":%lu,\"lap\":%lu,\"eventId\":%lu}\n\n",
                        (unsigned long)revision, (unsigned long long)systemTime(),
                        static_cast<unsigned long>(payloadEntry), static_cast<unsigned long>(payloadLap),
                        static_cast<unsigned long>(payloadEventId));
    } else if (controlPending) {
      length = snprintf(session->payloadFrame, sizeof(session->payloadFrame), "event: control\ndata: {\"kind\":\"TAKEOVER\",\"requestId\":%lu}\n\n",
                        static_cast<unsigned long>(controlRequestId));
    } else if (controlRefreshPending) {
      length = snprintf(session->payloadFrame, sizeof(session->payloadFrame), "event: control\ndata: {\"kind\":\"CONTROL_CHANGED\"}\n\n");
    } else {
      const auto& p = presentation_;
      length = snprintf(session->payloadFrame, sizeof(session->payloadFrame),
        "event: presentation\ndata: {\"l\":\"%s\",\"m\":\"%s\",\"t\":%llu,\"r\":%llu,\"o\":%llu,\"x\":%s,\"p\":\"%s\",\"n\":%u,\"c\":%u,\"g\":%s}\n\n",
        lifecycle(p.lifecycle), setupMode(p.sessionMode),
        (unsigned long long)systemTime(),
        (unsigned long long)p.remainingDuration, (unsigned long long)p.overtime,
        p.durationExpired ? "true" : "false", startPhase(p.startPhase),
        unsigned(p.redLightsLit), unsigned(p.redLightCount),
        p.startLightsGreen ? "true" : "false");
    }
    const uint32_t sendStarted = millis();
    const esp_err_t sent = length > 0
      ? httpd_resp_send_chunk(session->asyncRequest, session->payloadFrame, size_t(length))
      : ESP_FAIL;
    const uint32_t sendElapsed = static_cast<uint32_t>(millis() - sendStarted);
    recordSseStack(session, keepalive ? "keepalive" : "notification", frameHwm);
    if (sendElapsed >= SseStalledSendThresholdMs) recordSseStack(session, "stalled_send", session->stackHwmStalled);
    if (sent != ESP_OK) {
      recordSseStack(session, "send_failure", session->stackHwmFailure);
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
      Serial.printf("[SSE] send_failed revision=%lu lap_entry=%lu event_id=%lu at_us=%llu result=%d\n",
        static_cast<unsigned long>(revision),
        static_cast<unsigned long>(lapNoticeRevision_ == revision ? lapNoticeEntry_ : 0),
        static_cast<unsigned long>(lapNoticeRevision_ == revision ? lapNoticeEventId_ : 0),
        static_cast<unsigned long long>(systemTime()), static_cast<int>(sent));
#endif
      invalidateSseSession(session, true);
    } else {
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
      Serial.printf("[SSE] sent generation=%lu revision=%lu lap_entry=%lu lap=%lu event_id=%lu at_us=%llu\n",
        static_cast<unsigned long>(session->generation),
        static_cast<unsigned long>(revision),
        static_cast<unsigned long>(lapNoticeRevision_ == revision ? lapNoticeEntry_ : 0),
        static_cast<unsigned long>(lapNoticeRevision_ == revision ? lapNoticeNumber_ : 0),
        static_cast<unsigned long>(lapNoticeRevision_ == revision ? lapNoticeEventId_ : 0),
        static_cast<unsigned long long>(systemTime()));
#endif
      if (noticePending) session->sentRevision = revision;
      if (!noticePending && !controlPending && !controlRefreshPending && !keepalive && presentationSequence == presentationSequence_) {
        session->sentPresentationSequence = presentationSequence;
      }
      if (controlPending && controlRequestId == session->controlRequestId) {
        session->controlPending = false;
      }
      if (controlRefreshPending) session->controlRefreshPending = false;
      session->lastSendMs = millis();
      if (findSseSessionExact(session) != session || !session->valid) {
        session->valid = false;
      } else {
        (void)httpd_sess_update_lru_counter(handle, fd);
      }
    }
  }
  static esp_err_t events(httpd_req_t* request) {
    RequestTrace trace(instance(), "/events", request, true);
    const uint64_t browserToken = client(request);
    BrowserInterface* browser = instance();
    const int fd = httpd_req_to_sockfd(request);
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
    Serial.printf("[SSE] handler_enter fd=%d revision=%lu at_us=%llu\n",
      fd, static_cast<unsigned long>(browser ? browser->noticeRevision_ : 0),
      static_cast<unsigned long long>(systemTime()));
#endif
    if (!browser || fd < 0) { if (browser) browser->transportAdmissionTrace("/events", fd, 0, 0, 0, "invalid"); trace.complete(ESP_ERR_INVALID_ARG); return ESP_FAIL; }
    if (SseSession* previous = browser->findSseSession(browserToken)) {
      browser->invalidateSseSession(previous, true, false, SseCleanupReason::Replacement);
    }
    size_t slotCount = 0;
    for (size_t i = 0; i < MaxSseClients; ++i) if (browser->sseSessions_[i]) ++slotCount;
    size_t fdsCapacity = 13;
    int clientFds[13]{};
    const esp_err_t clientListResult = browser->server_
      ? httpd_get_client_list(browser->server_, &fdsCapacity, clientFds)
      : ESP_ERR_INVALID_STATE;
    browser->transportAdmissionTrace("/events", fd, fdsCapacity, slotCount, 0,
                                     clientListResult == ESP_OK ? "checked" : "client_list_failed");
    if (clientListResult != ESP_OK || fdsCapacity >= MaxHttpClientSockets - OrdinaryRequestReserve) {
      browser->transportAdmissionTrace("/events", fd, fdsCapacity, slotCount, 0,
                                       clientListResult == ESP_OK ? "ordinary_capacity_exhausted" : "client_list_failed");
      httpd_resp_set_status(request, "503 Service Unavailable");
      trace.complete(clientListResult == ESP_OK ? ESP_ERR_NO_MEM : clientListResult);
      return ESP_FAIL;
    }
    if (slotCount >= MaxSseClients) {
      browser->transportAdmissionTrace("/events", fd, fdsCapacity, slotCount, 0, "sse_slots_exhausted");
      httpd_resp_set_status(request, "503 Service Unavailable");
      trace.complete(ESP_ERR_NO_MEM);
      return ESP_FAIL;
    }
    auto* session = browser->allocateSseSession(browserToken);
    if (!session) {
      browser->transportAdmissionTrace("/events", fd, fdsCapacity, slotCount, 0, "session_allocation_failed");
      trace.complete(ESP_ERR_NO_MEM); return ESP_FAIL;
    }
#if PP_HTTP_ASYNC_RUNTIME
    if (!session->worker) {
      browser->transportAdmissionTrace("/events", fd, fdsCapacity, slotCount, 0, "worker_unavailable");
      browser->removeSseSession(session);
      browser->completeSseRequest(session);
      httpd_resp_set_status(request, "503 Service Unavailable");
      trace.complete(ESP_ERR_NO_MEM);
      return ESP_FAIL;
    }
#endif
    httpd_req_t* asyncRequest = nullptr;
#if PP_HTTP_ASYNC_RUNTIME
    const esp_err_t asyncResult = httpd_req_async_handler_begin(request, &asyncRequest);
    if (asyncResult != ESP_OK || !asyncRequest) {
      browser->transportAdmissionTrace("/events", fd, fdsCapacity, slotCount, 0, "async_begin_failed");
      browser->removeSseSession(session);
      browser->completeSseRequest(session);
      httpd_resp_set_status(request, "503 Service Unavailable");
      trace.complete(asyncResult);
      return ESP_FAIL;
    }
#else
    // IDF 4.x has no async-request ownership API. Preserve the historical
    // synchronous compatibility path for those legacy acceptance builds; the
    // migrated IDF 5.x production path never enters this branch.
    asyncRequest = request;
#endif
    httpd_resp_set_type(asyncRequest, "text/event-stream");
    httpd_resp_set_hdr(asyncRequest, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(asyncRequest, "Connection", "keep-alive");
    session->fd = fd;
    session->asyncRequest = asyncRequest;
    session->initialPending = true;
    httpd_sess_set_ctx(browser->server_, fd, session, &BrowserInterface::freeSseSession);
    session->sentRevision = browser->noticeRevision_;
    session->sentPresentationSequence = browser->presentationSequence_;
    session->lastSendMs = millis();
    browser->controlConnected(browserToken);
    browser->queueEventWork(session);
    browser->transportAdmissionTrace("/events", fd, fdsCapacity, slotCount + 1, 0, "admitted");
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
    if (strstr(value, "PRACTICE_AGAIN")) return diagnostic::OperationName::PracticeAgain;
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
    char kind[32]{};
    char lifecycle[20]{};
    char mode[20]{};
    char operation[32]{};
    char phase[24]{};
    diagnostic::jsonTextField(body, "\"kind\"", kind, sizeof(kind));
    diagnostic::jsonTextField(body, "\"lifecycle\"", lifecycle, sizeof(lifecycle));
    diagnostic::jsonTextField(body, "\"sessionMode\"", mode, sizeof(mode));
    diagnostic::jsonTextField(body, "\"operation\"", operation, sizeof(operation));
    diagnostic::jsonTextField(body, "\"phase\"", phase, sizeof(phase));
    const uint64_t traceId = number64("\"diagnosticTraceId\"");
    const uint32_t visible = number("\"correlationId\"");
    const uint32_t revision = number("\"revision\"");
    const uint32_t eventId = number("\"eventId\"");
    const uint32_t detail = number("\"detail\"");
    // Targeted Wi-Fi fault-finding instrumentation.  These observations are
    // deliberately credential-free and are emitted to the host log so a
    // Browser-side gate can be distinguished from an HTTP/API failure even
    // when the diagnostic SD sink is unavailable.
    if (strcmp(kind, "network_control_state") == 0 ||
        strcmp(kind, "network_reconfigure_click") == 0) {
      Serial.printf("[WIFI] BROWSER_OBSERVATION kind=%s detail=%lu client_is_master=%s\n",
                    kind, static_cast<unsigned long>(detail),
                    instance()->isControlOwner(owner) ? "true" : "false");
    }
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
      if (diagnostic::browserStateObservationKind(kind, event)) {
        browser->diagnosticTrace_->observeBrowserState(
            event, clientId, traceId, visible, revision,
            lifecycleCode, modeCode, observationDetail, op, systemTime());
      }
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
    RequestTrace trace(instance(), "/context", request, true);
    const uint64_t owner = client(request);
    const uint32_t clientTag = instance()->contextClientTag(owner);
    char json[320];
    const ControlLeaseResult result = instance()->controlResultFor(owner);
    const bool hasMaster = instance()->hasMaster();
    const bool isMaster = instance()->isControlOwner(owner);
    const bool ownerConnected = instance()->controlOwnerConnected();
    const bool takeoverPending = instance()->controlTakeoverPending();
    const bool takeoverRequestedByThis = instance()->controlTakeoverRequestedBy(owner);
    snprintf(json, sizeof(json), "{\"role\":\"%s\",\"hasMaster\":%s,\"isMaster\":%s,\"ownerConnected\":%s,\"takeoverPending\":%s,\"takeoverRequestedByThis\":%s,\"takeoverRequestId\":%lu,\"takeoverResult\":\"%s\"}",
             instance()->context(owner) == ClientContext::RaceDirectorSmug ? "Race Director" : "Spectator",
             hasMaster ? "true" : "false",
             isMaster ? "true" : "false",
             ownerConnected ? "true" : "false",
             takeoverPending ? "true" : "false",
             takeoverRequestedByThis ? "true" : "false",
             static_cast<unsigned long>(takeoverPending ? instance()->controlTakeoverRequestId() : 0),
             controlResultText(result));
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    trace.sendBegin();
    const esp_err_t sent = httpd_resp_sendstr(request, json);
    trace.sendComplete(sent);
    instance()->traceContextResponse(clientTag, hasMaster, isMaster, ownerConnected,
                                     takeoverPending, takeoverRequestedByThis,
                                     result, sent);
    trace.complete(sent); return sent;
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
    RequestTrace trace(instance(), "/proposal", request, true);
    client(request);BrowserInterface* browser=instance();char* json=static_cast<char*>(malloc(4096));size_t length=0;if(!json){httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"proposal buffer unavailable");trace.complete(ESP_ERR_NO_MEM);return ESP_FAIL;}if(!serializeProposal(browser->proposedRaceSetup(),browser->controllerEpoch_,browser->proposalRevision(),browser->readiness(),json,4096,length)){free(json);httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"proposal unavailable");trace.complete(ESP_ERR_INVALID_SIZE);return ESP_FAIL;}httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");trace.sendBegin();const esp_err_t sent=httpd_resp_sendstr(request,json);trace.sendComplete(sent);free(json);trace.complete(sent);return sent;
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
  static esp_err_t practiceAgainRoute(httpd_req_t* r){return operationRoute(r,SessionOperation::PracticeAgain);}
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
#if defined(PP_PHASE8B_P1_TEST) || defined(PP_STAGE11_DEMO) || defined(PP_STAGE11_ACCEPTANCE) || defined(PP_STAGE12_DEMO) || defined(PP_STAGE12_ACCEPTANCE) || defined(PP_STAGE13_DEMO) || defined(PP_STAGE13_ACCEPTANCE) || defined(PP_STAGE14A_DEMO) || defined(PP_STAGE14A_ACCEPTANCE) || defined(PP_STAGE14B_DEMO) || defined(PP_STAGE14B_ACCEPTANCE) || defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
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
#if PP_HTTP_ASYNC_RUNTIME
    BrowserInterface* browser = instance();
    if (!browser || !browser->asyncWorkersStarted_ || !browser->pageQueue_ || uxQueueSpacesAvailable(browser->pageQueue_) == 0) {
      if (browser) browser->transportAdmissionTrace("/", httpd_req_to_sockfd(request), browser->httpClientCount(), 0,
                                                     browser->pageQueue_ ? uxQueueSpacesAvailable(browser->pageQueue_) : 0,
                                                     "page_queue_exhausted");
      httpd_resp_set_status(request, "503 Service Unavailable");
      return httpd_resp_sendstr(request, "page capacity unavailable");
    }
    // Keep the ordinary-request reserve explicit for admitted page jobs as
    // well as SSE.  The request itself is already present in the HTTPD client
    // list, so no more than (client capacity - reserve) clients may be in
    // admitted page/SSE work at once.
    size_t fdsCapacity = 13;
    int clientFds[13]{};
    const esp_err_t clientListResult = browser->server_
      ? httpd_get_client_list(browser->server_, &fdsCapacity, clientFds)
      : ESP_ERR_INVALID_STATE;
    if (clientListResult != ESP_OK || fdsCapacity >= MaxHttpClientSockets - OrdinaryRequestReserve) {
      browser->transportAdmissionTrace("/", httpd_req_to_sockfd(request), fdsCapacity, 0,
                                       browser->pageQueue_ ? uxQueueSpacesAvailable(browser->pageQueue_) : 0,
                                       clientListResult == ESP_OK ? "ordinary_capacity_exhausted" : "client_list_failed");
      httpd_resp_set_status(request, "503 Service Unavailable");
      return httpd_resp_sendstr(request, "page ordinary capacity unavailable");
    }
    httpd_req_t* asyncRequest = nullptr;
    const esp_err_t beginResult = httpd_req_async_handler_begin(request, &asyncRequest);
    if (beginResult != ESP_OK || !asyncRequest) {
      browser->transportAdmissionTrace("/", httpd_req_to_sockfd(request), fdsCapacity, 0,
                                       browser->pageQueue_ ? uxQueueSpacesAvailable(browser->pageQueue_) : 0,
                                       "async_begin_failed");
      httpd_resp_set_status(request, "503 Service Unavailable");
      return httpd_resp_sendstr(request, "page request unavailable");
    }
    PageJob job{};
    job.owner = browser;
    job.server = browser->server_;
    job.fd = httpd_req_to_sockfd(asyncRequest);
    job.asyncRequest = asyncRequest;
    job.generation = ++browser->nextPageGeneration_;
    job.serverGeneration = browser->serverGeneration_;
    if (xQueueSend(browser->pageQueue_, &job, 0) != pdTRUE) {
      browser->transportAdmissionTrace("/", job.fd, fdsCapacity, 0, 0, "page_queue_full");
      browser->completeAsyncRequest(asyncRequest);
      httpd_resp_set_status(request, "503 Service Unavailable");
      return ESP_FAIL;
    }
    browser->transportAdmissionTrace("/", job.fd, fdsCapacity, 0,
                                     uxQueueSpacesAvailable(browser->pageQueue_), "admitted");
    return ESP_OK;
#else
    return sendNormalPage(request);
#endif
  }
  static esp_err_t sendNormalPage(httpd_req_t* request) {
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
    const diagnostic::RecorderResult result = browser->diagnosticTrace_->startDevelopment(start, status);
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
    const diagnostic::RecorderResult result = browser->diagnosticTrace_->stopDevelopment(status);
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
    if (!browser->diagnosticTrace_) {
      httpd_resp_set_status(request, "503 Service Unavailable");
      httpd_resp_sendstr(request, "diagnostics unavailable");
      trace.complete(ESP_ERR_INVALID_STATE);
      return ESP_FAIL;
    }
    diagnostic::SessionStatus session{};
    diagnostic::SessionStatus normal{};
    diagnostic::StorageStatus storage{};
    size_t retained = 0;
    if (!browser->diagnosticTrace_->developmentStatus(session)) {
      trace.complete(ESP_ERR_INVALID_STATE);
      return ESP_FAIL;
    }
    if (!browser->diagnosticTrace_->status(normal, storage, retained)) {
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
      "{\"formatVersion\":1,\"controllerEpoch\":%llu,\"session\":%llu,\"lifecycle\":%u,\"developmentSession\":%llu,\"developmentLifecycle\":%u,\"storageHealth\":%u,\"backend\":%u,\"lastFailure\":%u,\"nextSequence\":%llu,\"bytesRecorded\":%llu,\"retainedRecords\":%u,\"droppedRecords\":%llu,\"evidenceVolatile\":%s,\"evidenceIncomplete\":%s,\"exportCapability\":\"%s\",\"retrievalAvailable\":%s}",
      static_cast<unsigned long long>(normal.controllerEpoch.value),
      static_cast<unsigned long long>(normal.session.value),
      static_cast<unsigned>(normal.lifecycle),
      static_cast<unsigned long long>(session.session.value), static_cast<unsigned>(session.lifecycle), static_cast<unsigned>(storage.health),
      static_cast<unsigned>(storage.backend), static_cast<unsigned>(storage.lastFailure),
      static_cast<unsigned long long>(normal.nextSequence), static_cast<unsigned long long>(normal.bytesRecorded), static_cast<unsigned>(retained),
      static_cast<unsigned long long>(normal.droppedRecords + session.droppedRecords), normal.evidenceVolatile ? "true" : "false",
      normal.evidenceIncomplete || session.evidenceIncomplete ? "true" : "false",
      diagnosticExportCapability(storage, retained), retained ? "true" : "false");
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
  static const char* diagnosticExportCapability(const diagnostic::StorageStatus& storage,
                                                size_t retained) {
    return diagnostic::DiagnosticExport::capability(storage, retained);
  }
  static const char* diagnosticBackendName(diagnostic::BackendKind backend) {
    switch (backend) {
      case diagnostic::BackendKind::Memory: return "RAM";
      case diagnostic::BackendKind::DevelopmentFile: return "DEVELOPMENT_FILE";
      case diagnostic::BackendKind::FailureInjection: return "FAILURE_INJECTION";
      case diagnostic::BackendKind::ProductionSd: return "ESP32_SPI_SD";
      default: return "UNKNOWN";
    }
  }
  class DiagnosticHttpSink final : public diagnostic::DiagnosticExportSink {
  public:
    explicit DiagnosticHttpSink(httpd_req_t* request) : request_(request) {}
    bool write(const char* data, size_t length) override {
      return httpd_resp_send_chunk(request_, data, length) == ESP_OK;
    }
  private:
    httpd_req_t* request_;
  };
  static esp_err_t diagnosticDownloadRoute(httpd_req_t* request) {
    RequestTrace trace(instance(), "/diag/download");
    BrowserInterface* browser = instance();
    if (!browser->diagnosticTrace_) {
      httpd_resp_set_status(request, "503 Service Unavailable");
      httpd_resp_sendstr(request, "diagnostics unavailable");
      trace.complete(ESP_ERR_INVALID_STATE);
      return ESP_FAIL;
    }
    diagnostic::SessionStatus development{};
    diagnostic::SessionStatus normal{};
    diagnostic::StorageStatus storage{};
    size_t retained = 0;
    if (!browser->diagnosticTrace_->developmentStatus(development) ||
        !browser->diagnosticTrace_->status(normal, storage, retained)) {
      httpd_resp_set_status(request, "503 Service Unavailable");
      httpd_resp_sendstr(request, "diagnostic export unavailable");
      trace.complete(ESP_ERR_INVALID_STATE);
      return ESP_FAIL;
    }
    const char* capability = diagnosticExportCapability(storage, retained);
    if (strcmp(capability, "UNAVAILABLE") == 0) {
      httpd_resp_set_status(request, "503 Service Unavailable");
      httpd_resp_sendstr(request, "diagnostic export unavailable");
      trace.complete(ESP_ERR_INVALID_STATE);
      return ESP_FAIL;
    }
    httpd_resp_set_type(request, "application/x-ndjson; charset=utf-8");
    httpd_resp_set_hdr(request, "Content-Disposition",
                       "attachment; filename=P-and-P-Diagnostics.txt");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    diagnostic::DiagnosticExportMetadata metadata{};
    metadata.firmwareBuild = browser->diagnosticBuildIdentity_;
    metadata.sourceCommit = browser->diagnosticSourceCommit_;
    metadata.buildEnvironment = browser->diagnosticBuildEnvironment_;
    metadata.appElfSha256 = browser->diagnosticAppElfSha256_;
    metadata.backend = diagnosticBackendName(storage.backend);
    metadata.capability = capability;
    metadata.storageHealth = static_cast<uint8_t>(storage.health);
    metadata.controllerEpoch = normal.controllerEpoch;
    metadata.normalSession = normal.session;
    metadata.developmentSession = development.session;
    metadata.developmentLifecycle = development.lifecycle;
    metadata.evidenceVolatile = normal.evidenceVolatile;
    metadata.evidenceIncomplete = normal.evidenceIncomplete ||
                                   development.evidenceIncomplete;
    DiagnosticHttpSink sink(request);
    diagnostic::DiagnosticExportResult result{};
    const bool encoded = diagnostic::DiagnosticExport::write(
        browser->diagnosticTrace_->recorder(), metadata,
        normal.droppedRecords + development.droppedRecords, sink, result);
    const esp_err_t sent = httpd_resp_send_chunk(request, nullptr, 0);
    const esp_err_t outcome = encoded && sent == ESP_OK ? ESP_OK : ESP_FAIL;
    trace.complete(outcome);
    return outcome;
  }
  enum class NetworkCredentialParseResult : uint8_t {
    Accepted,
    MissingBody,
    BodyTooLarge,
    ReceiveFailed,
    SsidMissing,
    SsidEmpty,
    PasswordMissing,
    ValueInvalid,
  };
  static const char* networkCredentialParseReason(NetworkCredentialParseResult result) {
    switch (result) {
      case NetworkCredentialParseResult::Accepted: return "ACCEPTED";
      case NetworkCredentialParseResult::MissingBody: return "BODY_MISSING";
      case NetworkCredentialParseResult::BodyTooLarge: return "BODY_TOO_LARGE";
      case NetworkCredentialParseResult::ReceiveFailed: return "BODY_RECEIVE_FAILED";
      case NetworkCredentialParseResult::SsidMissing: return "SSID_MISSING";
      case NetworkCredentialParseResult::SsidEmpty: return "SSID_EMPTY";
      case NetworkCredentialParseResult::PasswordMissing: return "PASSWORD_MISSING";
      case NetworkCredentialParseResult::ValueInvalid: return "VALUE_INVALID";
    }
    return "INVALID_REQUEST";
  }
  static NetworkCredentialParseResult networkCredentialsFromBody(httpd_req_t* request, char* ssid, size_t ssidCapacity,
                                                                 char* password, size_t passwordCapacity) {
    if (!ssid || !password || request->content_len <= 0) return NetworkCredentialParseResult::MissingBody;
    if (request->content_len >= 256) return NetworkCredentialParseResult::BodyTooLarge;
    char body[256]{};
    int received = 0;
    while (received < request->content_len) {
      const int part = httpd_req_recv(request, body + received, request->content_len - received);
      if (part <= 0) return NetworkCredentialParseResult::ReceiveFailed;
      received += part;
    }
    body[received] = 0;
    const auto value = [&](const char* key, char* out, size_t capacity) {
      out[0] = 0;
      const char* start = strstr(body, key);
      start = start ? strchr(start + strlen(key), ':') : nullptr;
      start = start ? strchr(start, '"') : nullptr;
      if (!start) return false;
      ++start;
      size_t used = 0;
      bool escaped = false;
      for (const char* p = start; *p; ++p) {
        if (escaped) { if (used + 1 < capacity) out[used++] = *p; escaped = false; continue; }
        if (*p == '\\') { escaped = true; continue; }
        if (*p == '"') { out[used] = 0; return true; }
        if (used + 1 >= capacity) return false;
        out[used++] = *p;
      }
      out[0] = 0;
      return false;
    };
    if (!value("\"ssid\"", ssid, ssidCapacity)) return NetworkCredentialParseResult::SsidMissing;
    if (ssid[0] == 0) return NetworkCredentialParseResult::SsidEmpty;
    if (!value("\"password\"", password, passwordCapacity)) return NetworkCredentialParseResult::PasswordMissing;
    return NetworkCredentialParseResult::Accepted;
  }
  static esp_err_t networkStatusRoute(httpd_req_t* request) {
    RequestTrace trace(instance(), "/network/status");
    BrowserInterface* browser = instance();
    NetworkStatus network{};
    browser->networkStatus(network);
    const auto& v = network.value;
    char staSsid[40]{};
    for (size_t i = 0; i + 1 < sizeof(staSsid) && i < v.staSsid.length(); ++i) {
      const char c = v.staSsid[i];
      staSsid[i] = (c == '\"' || c == '\\' || static_cast<unsigned char>(c) < 0x20) ? '_' : c;
    }
    char json[4096]{};
    size_t used = static_cast<size_t>(snprintf(json, sizeof(json),
      "{\"available\":%s,\"ap\":{\"started\":%s,\"ssid\":\"%s\",\"ip\":\"%s\",\"clients\":%u},\"sta\":{\"configured\":%s,\"state\":\"%s\",\"connected\":%s,\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%ld,\"lastFailure\":\"%s\",\"reconnectSuppressed\":%s,\"reconnectAttempts\":%lu},\"mdns\":{\"started\":%s,\"host\":\"%s\"},\"scan\":{\"state\":\"%s\",\"running\":%s,\"count\":%d,\"requests\":%lu,\"accepted\":%lu,\"completed\":%lu,\"unavailable\":%lu,\"lastResult\":%d,\"networks\":[",
      v.apStarted || v.staConfigured ? "true" : "false", v.apStarted ? "true" : "false",
      ExternalWifiManager::FactoryApSsid, v.apIp.c_str(), static_cast<unsigned>(v.apClients),
      v.staConfigured ? "true" : "false", ExternalWifiManager::staStateName(v.staState),
      v.staConnected ? "true" : "false", staSsid, v.staIp.c_str(), static_cast<long>(v.staRssi),
      ExternalWifiManager::staFailureName(v.staFailure), v.reconnectSuppressed ? "true" : "false",
      static_cast<unsigned long>(v.reconnectAttempts), v.mdnsStarted ? "true" : "false",
      ExternalWifiManager::MdnsHost, ExternalWifiManager::scanStateName(v.scanState),
      v.scanRunning ? "true" : "false", static_cast<int>(v.scanCount),
      static_cast<unsigned long>(v.scanRequests), static_cast<unsigned long>(v.scanAccepted),
      static_cast<unsigned long>(v.scanCompleted), static_cast<unsigned long>(v.scanUnavailable),
      static_cast<int>(v.scanLastResult)));
    if (used >= sizeof(json)) { httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "network status unavailable"); trace.complete(ESP_ERR_INVALID_SIZE); return ESP_FAIL; }
#if defined(PP_PHASE8B_REAL_SD)
    if (v.scanCount > 0) {
      for (int16_t i = 0; i < v.scanCount; ++i) {
        String ssid = WiFi.SSID(static_cast<uint8_t>(i));
        char safeSsid[40]{};
        for (size_t n = 0; n + 1 < sizeof(safeSsid) && n < ssid.length(); ++n) {
          const char c = ssid[n];
          safeSsid[n] = (c == '\"' || c == '\\' || static_cast<unsigned char>(c) < 0x20) ? '_' : c;
        }
        const int written = snprintf(json + used, sizeof(json) - used,
          "%s{\"ssid\":\"%s\",\"rssi\":%ld,\"secure\":%s}", i ? "," : "", safeSsid,
          static_cast<long>(WiFi.RSSI(static_cast<uint8_t>(i))),
          WiFi.encryptionType(static_cast<uint8_t>(i)) == WIFI_AUTH_OPEN ? "false" : "true");
        if (written < 0 || static_cast<size_t>(written) >= sizeof(json) - used) { httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "network status unavailable"); trace.complete(ESP_ERR_INVALID_SIZE); return ESP_FAIL; }
        used += static_cast<size_t>(written);
      }
    }
#endif
    const int written = snprintf(json + used, sizeof(json) - used, "]}}");
    if (written < 0 || static_cast<size_t>(written) >= sizeof(json) - used) { httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "network status unavailable"); trace.complete(ESP_ERR_INVALID_SIZE); return ESP_FAIL; }
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    const esp_err_t sent = httpd_resp_send(request, json, HTTPD_RESP_USE_STRLEN);
    trace.complete(sent);
    return sent;
  }
  static esp_err_t networkScanRoute(httpd_req_t* request) {
    RequestTrace trace(instance(), "/network/scan");
    Serial.println("[WIFI] SCAN_REQUEST route_received=true");
    const bool accepted = instance()->networkStartScan();
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_status(request, accepted ? "202 Accepted" : "409 Conflict");
    const esp_err_t sent = httpd_resp_sendstr(request, accepted ? "{\"accepted\":true}" : "{\"accepted\":false,\"reason\":\"SCAN_UNAVAILABLE\"}");
    trace.complete(sent);
    return sent;
  }
  static esp_err_t networkConnectRoute(httpd_req_t* request) {
    RequestTrace trace(instance(), "/network/connect");
    const uint64_t owner = client(request);
    const bool hasMaster = instance()->hasMaster();
    const bool isMaster = instance()->isControlOwner(owner);
    const bool ownerConnected = instance()->controlOwnerConnected();
    const ClientContext context = instance()->context(owner);
    Serial.printf("[WIFI] RECONFIGURE_REQUEST route_received=true has_master=%s is_master=%s owner_connected=%s context=%s\n",
                  hasMaster ? "true" : "false", isMaster ? "true" : "false",
                  ownerConnected ? "true" : "false",
                  context == ClientContext::RaceDirectorSmug ? "RACE_DIRECTOR" : "SPECTATOR");
    if (context != ClientContext::RaceDirectorSmug) {
      Serial.println("[WIFI] RECONFIGURE_REJECTED reason=RACE_DIRECTOR_REQUIRED");
      httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "Race Director required");
      trace.complete(ESP_ERR_INVALID_STATE);
      return ESP_FAIL;
    }
    char ssid[33]{}, password[65]{};
    const NetworkCredentialParseResult parsed = networkCredentialsFromBody(request, ssid, sizeof(ssid), password, sizeof(password));
    if (parsed != NetworkCredentialParseResult::Accepted) {
      const char* reason = networkCredentialParseReason(parsed);
      Serial.printf("[WIFI] RECONFIGURE_REJECTED reason=%s ssid_length=%u password_present=%s\n",
                    reason, static_cast<unsigned>(strlen(ssid)), parsed == NetworkCredentialParseResult::PasswordMissing ? "false" : "unknown");
      httpd_resp_set_type(request, "application/json");
      httpd_resp_set_status(request, "400 Bad Request");
      char response[128]{};
      snprintf(response, sizeof(response), "{\"accepted\":false,\"reason\":\"INVALID_REQUEST\",\"detail\":\"%s\"}", reason);
      httpd_resp_sendstr(request, response);
      trace.complete(ESP_ERR_INVALID_ARG);
      return ESP_FAIL;
    }
    Serial.printf("[WIFI] RECONFIGURE_ACCEPTED ssid_length=%u password_present=true password_length=%u\n",
                  static_cast<unsigned>(strlen(ssid)),
                  static_cast<unsigned>(strlen(password)));
    const bool accepted = instance()->networkConnect(ssid, password);
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_status(request, accepted ? "202 Accepted" : "409 Conflict");
    const esp_err_t sent = httpd_resp_sendstr(request, accepted ? "{\"accepted\":true}" : "{\"accepted\":false,\"reason\":\"CONNECT_FAILED\"}");
    if (!accepted) Serial.println("[WIFI] RECONFIGURE_REJECTED reason=CONNECT_UNAVAILABLE");
    trace.complete(sent);
    return sent;
  }
  static esp_err_t networkReconnectRoute(httpd_req_t* request) {
    RequestTrace trace(instance(), "/network/reconnect");
    if (instance()->context(client(request)) != ClientContext::RaceDirectorSmug) {
      httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "Race Director required");
      trace.complete(ESP_ERR_INVALID_STATE);
      return ESP_FAIL;
    }
    const bool accepted = instance()->networkReconnect();
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_status(request, accepted ? "202 Accepted" : "409 Conflict");
    const esp_err_t sent = httpd_resp_sendstr(request, accepted ? "{\"accepted\":true}" : "{\"accepted\":false,\"reason\":\"RECONNECT_UNAVAILABLE\"}");
    trace.complete(sent);
    return sent;
  }
  static esp_err_t networkDisconnectRoute(httpd_req_t* request) {
    RequestTrace trace(instance(), "/network/disconnect");
    if (instance()->context(client(request)) != ClientContext::RaceDirectorSmug) { httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "Race Director required"); trace.complete(ESP_ERR_INVALID_STATE); return ESP_FAIL; }
    const bool accepted = instance()->networkDisconnect();
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_status(request, accepted ? "200 OK" : "409 Conflict");
    const esp_err_t sent = httpd_resp_sendstr(request, accepted ? "{\"accepted\":true}" : "{\"accepted\":false,\"reason\":\"DISCONNECT_FAILED\"}");
    trace.complete(sent);
    return sent;
  }
  static esp_err_t networkForgetRoute(httpd_req_t* request) {
    RequestTrace trace(instance(), "/network/forget");
    if (instance()->context(client(request)) != ClientContext::RaceDirectorSmug) { httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "Race Director required"); trace.complete(ESP_ERR_INVALID_STATE); return ESP_FAIL; }
    const bool accepted = instance()->networkForget();
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_status(request, accepted ? "200 OK" : "409 Conflict");
    const esp_err_t sent = httpd_resp_sendstr(request, accepted ? "{\"accepted\":true}" : "{\"accepted\":false,\"reason\":\"FORGET_FAILED\"}");
    trace.complete(sent);
    return sent;
  }
  static esp_err_t health(httpd_req_t* request) {
    RequestTrace trace(instance(), "/health", request, true);
    httpd_resp_set_type(request, "application/json");
    char body[512]{};
    snprintf(body, sizeof(body),
             "{\"ok\":true,\"browser\":\"stage13\",\"firmwareBuild\":\"%s\","
             "\"sourceCommit\":\"%s\",\"buildEnvironment\":\"%s\","
             "\"appElfSha256\":\"%s\"}",
             instance()->diagnosticBuildIdentity_, instance()->diagnosticSourceCommit_,
             instance()->diagnosticBuildEnvironment_, instance()->diagnosticAppElfSha256_);
    trace.sendBegin();
    const esp_err_t sent = httpd_resp_sendstr(request, body);
    trace.sendComplete(sent); trace.complete(sent); return sent;
  }
  void startServer() {
    instance() = this;
    static const httpd_uri_t routes[] = {
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
      {"/diag/download", HTTP_GET, diagnosticDownloadRoute, nullptr},
      {"/network/status", HTTP_GET, networkStatusRoute, nullptr},
      {"/network/scan", HTTP_POST, networkScanRoute, nullptr},
      {"/network/connect", HTTP_POST, networkConnectRoute, nullptr},
      {"/network/reconnect", HTTP_POST, networkReconnectRoute, nullptr},
      {"/network/disconnect", HTTP_POST, networkDisconnectRoute, nullptr},
      {"/network/forget", HTTP_POST, networkForgetRoute, nullptr},
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
      {"/request/practice-again", HTTP_POST, practiceAgainRoute, nullptr},
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
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 8192;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    // ESP32 Arduino's configured LWIP limit permits at most 13 sockets,
    // including ESP-IDF's three internal sockets.  SSE admission reserves
    // ordinary-request capacity; LRU eviction remains a last-resort policy.
    config.max_open_sockets = 13;
    config.lru_purge_enable = true;
    config.max_uri_handlers = static_cast<uint16_t>(sizeof(routes) / sizeof(routes[0]));
    const esp_err_t started = httpd_start(&server_, &config);
    if (started != ESP_OK) {
      serverStartError_ = int(started);
      server_ = nullptr;
      return;
    }
    ++serverGeneration_;
    if (!serverGeneration_) ++serverGeneration_;
    if (!startHttpWorkers()) {
      serverStartError_ = int(ESP_ERR_NO_MEM);
      httpd_stop(server_);
      server_ = nullptr;
      return;
    }
    for (const httpd_uri_t& route : routes) {
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
    for (size_t i = 0; i < MaxSseClients; ++i) {
      SseSession* session = sseSessions_[i];
      if (!session) continue;
      session->valid = false;
      session->transportClosed = true;
      session->cleanupReason = SseCleanupReason::Shutdown;
      removeSseSession(session);
      controlDisconnected(session->clientToken);
      const esp_err_t closeResult = httpd_sess_trigger_close(session->server, session->fd);
      if (closeResult != ESP_OK && closeResult != ESP_ERR_NOT_FOUND) {
        Serial.printf("[SSE] close_failed generation=%lu fd=%d result=%d\n",
                      static_cast<unsigned long>(session->generation), session->fd,
                      static_cast<int>(closeResult));
      }
#if PP_HTTP_ASYNC_RUNTIME
      if (session->worker) xTaskNotifyGive(session->worker);
#endif
    }
    ++serverStops_;
    const httpd_handle_t old = server_;
    server_ = nullptr;
    const esp_err_t stopped = httpd_stop(old);
    if (stopped != ESP_OK) serverStartError_ = int(stopped);
  }
};
}  // namespace pp
