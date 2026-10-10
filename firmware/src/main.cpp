#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "pp/system_time.h"
#include <stdarg.h>
#include "pp/core.h"
#include "pp/session_definition.h"
#include "pp/race_control.h"
#include "pp/race_engine.h"
#include "pp/history_store.h"
#include "pp/record_store.h"
#include "pp/noticeboard.h"
#include "pp/browser_interface.h"
#include "pp/verification.h"
#include "pp/persistent_storage.h"
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
    char generated[16]{};const char* key=s==0?"config-a":s==1?"config-b":(snprintf(generated,sizeof(generated),"slot-%u",s),generated);
    if(!preferences.isKey(key))return false;
    auto length=preferences.getBytesLength(key);
    if(!length)return false;
    // Preserve the distinction between absent and malformed records.
    if(length!=n){memset(data,0,n);return true;}
    return preferences.getBytes(key,data,n)==n;
  }
  bool write(unsigned s,const uint8_t* data,size_t n)override{char generated[16]{};const char* key=s==0?"config-a":s==1?"config-b":(snprintf(generated,sizeof(generated),"slot-%u",s),generated);return ready&&preferences.putBytes(key,data,n)==n;}
private: Preferences preferences;bool ready=false;
} storage;
class BrowserAuthorityStore final: public pp::AuthorityStore {
public:
  void begin(){ready=preferences.begin("pp-browser-auth",false);}
  bool loadMaster(uint64_t& value) override { if(!ready||!preferences.isKey("master"))return false;value=preferences.getULong64("master",0);return value!=0; }
  bool saveMaster(uint64_t value) override { return ready&&value&&preferences.putULong64("master",value)==sizeof(value); }
  void clearForFixture(){if(ready&&preferences.isKey("master"))preferences.remove("master");}
private: Preferences preferences;bool ready=false;
} browserAuthority;
pp::Bus bus;
pp::Memory memory(storage);
pp::SlotHistoryStore rawHistory(storage);
pp::VersionedHistoryStore history(rawHistory);
pp::VersionedTrackRecordStore records(storage);
const auto lifecycle=bus.attach(pp::Role::Lifecycle);
const auto memoryEndpoint=bus.attach(pp::Role::Memory);
pp::InputModule input(bus,bus.attach(pp::Role::Input));
pp::ActiveSessionDefinition activeSession;
const auto raceControlEndpoint=bus.attach(pp::Role::RaceControl);
pp::RaceControlModule raceControl(bus,raceControlEndpoint,activeSession,&history,&records);
const auto raceEngineEndpoint=bus.attach(pp::Role::RaceEngine);
pp::RaceEngineModule raceEngine(bus,raceEngineEndpoint,activeSession,&history,&records);
pp::OutputModule output{bus.attach(pp::Role::Output)};
const auto presentationEndpoint=bus.attach(pp::Role::Presentation);
pp::Noticeboard noticeboard(raceControl,raceEngine,activeSession);
pp::BrowserInterface browser(bus,presentationEndpoint,noticeboard,&browserAuthority,&history,&records);
const auto testPublisher=bus.attach(pp::Role::Diagnostics);
const auto testObserver=bus.attach(pp::Role::Diagnostics);
const auto inputObserver=bus.attach(pp::Role::Diagnostics);
pp::MemoryModule memoryModule(bus,memoryEndpoint,memory);
pp::Configuration workingConfiguration;
pp::ProposedRaceSetup proposedRaceSetup{ {pp::InputModule::simulatedDetectorIdentity(),1,pp::InputRole::StartFinish}, 1, 10, pp::LapFinishBehaviour::Immediate, 1, true, false };
// The development setup keeps a valid ten-minute Endurance value available
// when the Browser changes mode; Lap Race ignores this field.
struct ProposalDefaultsInitialiser { ProposalDefaultsInitialiser(){ proposedRaceSetup.durationMinutes=10; } } proposalDefaultsInitialiser;
bool ready=false,testsPassed=true,bootFailed=false;
pp::Time nextStatus=0;
pp::Time loopWorstUs=0;
pp::SessionLifecycle lastLoggedLifecycle=pp::SessionLifecycle::Faulted;
bool lifecycleLogKnown=false;
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
bool diagnosticExpiryEligible[pp::RaceEngineModule::MaxEntries]{};
bool diagnosticExpirySettled[pp::RaceEngineModule::MaxEntries]{};
bool diagnosticAllEligibleSettledLogged=false;
bool presentationLogKnown=false;
pp::SessionLifecycle lastPresentationLifecycle=pp::SessionLifecycle::Faulted;
pp::LapFinishBehaviour lastPresentationFinish=pp::LapFinishBehaviour::Immediate;
bool lastPresentationDurationExpired=false,lastPresentationOvertimeVisible=false;
bool networkDiagnosticKnown=false;
bool lastNetworkWifi=false,lastNetworkHttp=false;
uint32_t lastNetworkReconnects=0,lastNetworkErrors=0,lastNetworkSlow=0,lastNetworkStartError=0;
#endif
#if defined(PP_STAGE11_DEMO) || defined(PP_STAGE11_ACCEPTANCE) || defined(PP_STAGE12_DEMO) || defined(PP_STAGE12_ACCEPTANCE) || defined(PP_STAGE13_DEMO) || defined(PP_STAGE13_ACCEPTANCE) || defined(PP_STAGE14A_DEMO) || defined(PP_STAGE14A_ACCEPTANCE) || defined(PP_STAGE14B_DEMO) || defined(PP_STAGE14B_ACCEPTANCE) || defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
// Human/demo fixture: a trigger is a momentary passage; production input semantics are unchanged.
pp::Time simulatedARelease=0,simulatedBRelease=0;
#endif
#if defined(PP_STAGE10_DEMO) || defined(PP_STAGE11_DEMO) || defined(PP_STAGE12_DEMO) || defined(PP_STAGE13_DEMO) || defined(PP_STAGE14A_DEMO) || defined(PP_STAGE14B_DEMO) || defined(PP_STAGE14C_DEMO)
bool stage10ServerReported=false;
#endif
bool browserWifiStateKnown=false,browserWifiState=false;
bool browserHttpStateKnown=false,browserHttpState=false;
#if defined(PP_STAGE11_DEMO) || defined(PP_STAGE11_ACCEPTANCE) || defined(PP_STAGE12_DEMO) || defined(PP_STAGE12_ACCEPTANCE) || defined(PP_STAGE13_DEMO) || defined(PP_STAGE13_ACCEPTANCE) || defined(PP_STAGE14A_DEMO) || defined(PP_STAGE14A_ACCEPTANCE) || defined(PP_STAGE14B_DEMO) || defined(PP_STAGE14B_ACCEPTANCE) || defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
bool browserFixturePass(uint8_t lane){
  // The demo trigger is only a momentary source at the normal Input Module
  // boundary.  Every active timing mode uses the same eligibility rule.
  if(raceControl.state()!=pp::SessionLifecycle::Racing){
    diagnostics.log("[INPUT] fixture_pass lane=%u disposition=REJECTED reason=NO_ACTIVE_RACING_SESSION",unsigned(lane));
    return false;
  }
  const auto mode=raceControl.mode();
  if(mode!=pp::SessionMode::LapRace&&mode!=pp::SessionMode::OpenPractice&&mode!=pp::SessionMode::Endurance){
    diagnostics.log("[INPUT] fixture_pass lane=%u disposition=REJECTED reason=UNSUPPORTED_SESSION_MODE",unsigned(lane));
    return false;
  }
  if(lane==1){input.setSimulatedSource(true);simulatedARelease=systemTime()+150000;}
  else if(lane==2){input.setSimulatedSourceB(true);simulatedBRelease=systemTime()+150000;}
  else { diagnostics.log("[INPUT] fixture_pass lane=%u disposition=REJECTED reason=INVALID_LANE",unsigned(lane)); return false; }
  diagnostics.log("[INPUT] fixture_pass lane=%u disposition=SOURCE_ASSERTED mode=%u release_us=%llu",unsigned(lane),unsigned(mode),static_cast<unsigned long long>(lane==1?simulatedARelease:simulatedBRelease));
  return true;
}
void browserFixtureReset(){ activeSession.clearForFixture(); raceControl.resetRaceForFixture(); raceControl.setProposedRaceSetup(proposedRaceSetup); raceEngine.resetForFixture(); browser.clearRaceForFixture(); diagnostics.log("[DEV] TEST RESET READY (Race Director retained)"); }
#endif
// The build environment is diagnostic identity only. It never supplies product
// State or changes P&P behaviour.
const char* buildIdentity(){
 #if defined(PP_STAGE10_DEMO)
  return "P&P STAGE 10 DEMO";
#elif defined(PP_STAGE11_DEMO)
  return "P&P STAGE 11 DEMO";
#elif defined(PP_STAGE12_ACCEPTANCE)
  return "P&P STAGE 12 ACCEPTANCE";
#elif defined(PP_STAGE13_ACCEPTANCE)
  return "P&P STAGE 13 ACCEPTANCE";
#elif defined(PP_STAGE14A_ACCEPTANCE)
  return "P&P STAGE 14A ACCEPTANCE";
#elif defined(PP_STAGE14B_ACCEPTANCE)
  return "P&P STAGE 14B ACCEPTANCE";
#elif defined(PP_STAGE14C_ACCEPTANCE)
  return "P&P STAGE 14C ACCEPTANCE";
#elif defined(PP_STAGE12_DEMO)
  return "P&P STAGE 12 DEMO";
#elif defined(PP_STAGE14A_DEMO)
   return "P&P STAGE 14A DEMO";
#elif defined(PP_STAGE14B_DEMO)
  return "P&P STAGE 14B DEMO";
#elif defined(PP_STAGE14C_DEMO)
  return "P&P STAGE 14C DEMO";
#elif defined(PP_STAGE13_DEMO)
  return "P&P STAGE 13 DEMO";
#elif defined(PP_STAGE11_ACCEPTANCE)
  return "P&P STAGE 11 ACCEPTANCE";
#elif defined(PP_STAGE10_ACCEPTANCE)
  return "P&P STAGE 10 ACCEPTANCE";
#elif defined(PP_STAGE9_DEMO)
  return "P&P STAGE 9 DEMO";
#elif defined(PP_STAGE9_ACCEPTANCE)
  return "P&P STAGE 9 ACCEPTANCE";
#elif defined(PP_STAGE8_ACCEPTANCE)
  return "P&P STAGE 8 ACCEPTANCE";
#elif defined(PP_STAGE7_DEMO)
  return "P&P STAGE 7 DEMO";
#elif defined(PP_STAGE7_ACCEPTANCE)
  return "P&P STAGE 7 ACCEPTANCE";
#elif defined(PP_STAGE6_DEMO)
  return "P&P STAGE 6 DEMO";
#elif defined(PP_STAGE6_ACCEPTANCE)
  return "P&P STAGE 6 ACCEPTANCE";
#elif defined(PP_STAGE5_ACCEPTANCE)
  return "P&P STAGE 5 ACCEPTANCE";
#elif defined(PP_STAGE4_ACCEPTANCE)
  return "P&P STAGE 4 ACCEPTANCE";
#elif defined(PP_STAGE3_ACCEPTANCE)
  return "P&P STAGE 3 ACCEPTANCE";
#elif defined(PP_STAGE2_ACCEPTANCE)
  return "P&P STAGE 2 ACCEPTANCE";
#elif defined(PP_ACCEPTANCE)
  return "P&P STAGE 1 ACCEPTANCE";
#elif defined(PP_VERIFY)
  return "P&P VERIFICATION";
#elif defined(PP_DIAGNOSTICS) && !PP_DIAGNOSTICS
  return "P&P QUIET";
#else
  return "P&P NORMAL";
#endif
}
#ifdef PP_VERIFY
bool verificationReboot=false;
pp::Time rebootAt=0;
#endif
void report(const char* name,bool passed){diagnostics.log("[TEST] %s %s",passed?"PASS":"FAIL",name);}
uint8_t diagnosticLane(const pp::InputIdentity& input);
const char* lifecycleName(pp::SessionLifecycle value);
const char* finishName(pp::LapFinishBehaviour value);
void resetDiagnosticExpiry(){
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
  for(uint8_t i=0;i<pp::RaceEngineModule::MaxEntries;++i){diagnosticExpiryEligible[i]=false;diagnosticExpirySettled[i]=false;}
  diagnosticAllEligibleSettledLogged=false;
#endif
}
void observeInputEvents() {
  pp::Message event;
  while(bus.receive(inputObserver,event)) {
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
    const uint8_t lane=diagnosticLane(event.input);
    const auto* definition=raceControl.definition();
    const uint8_t index=lane?static_cast<uint8_t>(lane-1):pp::RaceEngineModule::MaxEntries;
    const auto* entry=(definition&&index<raceEngine.entryCount())?&raceEngine.entryState(index):nullptr;
    const bool enduranceAfter=definition&&definition->mode()==pp::SessionMode::Endurance&&raceControl.durationExpired()&&raceControl.durationExpiryAt()&&event.relevantTime>raceControl.durationExpiryAt();
    if(enduranceAfter){
      const bool eligible=index<pp::RaceEngineModule::MaxEntries&&diagnosticExpiryEligible[index];
      const bool settledBefore=index<pp::RaceEngineModule::MaxEntries?diagnosticExpirySettled[index]:(entry&&entry->postExpiryCompleted);
      const uint32_t afterLaps=entry?entry->laps:0;
      const bool finalAccepted=eligible&&!settledBefore&&entry&&entry->postExpiryCompleted;
      const char* decision=finalAccepted?"FINAL_LAP_ACCEPTED":settledBefore?"ALREADY_SETTLED_IGNORE":!eligible?"NOT_ELIGIBLE_IGNORE":"OTHER_IGNORE";
      const uint32_t beforeLaps=finalAccepted&&afterLaps?afterLaps-1:afterLaps;
      const uint32_t penalty=entry?entry->lapPenalty:0;
      const uint32_t beforeClassified=beforeLaps>penalty?beforeLaps-penalty:0;
      const uint32_t afterClassified=afterLaps>penalty?afterLaps-penalty:0;
      diagnostics.log("[ENDURANCE] post_expiry_crossing lane=%u entry=%lu crossing_us=%llu expiry_us=%llu eligible=%s settled_before=%s decision=%s factual_before=%lu factual_after=%lu classified_before=%lu classified_after=%lu settled_after=%s",
        unsigned(lane),entry?static_cast<unsigned long>(entry->raceEntryId):0UL,static_cast<unsigned long long>(event.relevantTime),static_cast<unsigned long long>(raceControl.durationExpiryAt()),eligible?"YES":"NO",settledBefore?"YES":"NO",decision,static_cast<unsigned long>(beforeLaps),static_cast<unsigned long>(afterLaps),static_cast<unsigned long>(beforeClassified),static_cast<unsigned long>(afterClassified),entry&&entry->postExpiryCompleted?"YES":"NO");
      if(finalAccepted&&!settledBefore){diagnosticExpirySettled[index]=true;diagnostics.log("[ENDURANCE] entry_settled entry=%lu crossing_us=%llu",entry?static_cast<unsigned long>(entry->raceEntryId):0UL,static_cast<unsigned long long>(event.relevantTime));}
      bool allSettled=true;bool anyEligible=false;for(uint8_t n=0;n<raceEngine.entryCount();++n){if(diagnosticExpiryEligible[n]){anyEligible=true;if(!diagnosticExpirySettled[n]){allSettled=false;break;}}}
      if(anyEligible&&allSettled&&!diagnosticAllEligibleSettledLogged){diagnosticAllEligibleSettledLogged=true;diagnostics.log("[ENDURANCE] all_eligible_entries_settled=YES relevant_us=%llu",static_cast<unsigned long long>(event.relevantTime));}
    }else if(!definition) diagnostics.log("[INPUT] crossing lane=%u relevant_us=%llu disposition=IGNORED reason=NO_SESSION",unsigned(lane),static_cast<unsigned long long>(event.relevantTime));
    else if(raceControl.state()==pp::SessionLifecycle::Paused) diagnostics.log("[INPUT] crossing lane=%u relevant_us=%llu disposition=IGNORED reason=PAUSED",unsigned(lane),static_cast<unsigned long long>(event.relevantTime));
    else if(entry&&entry->postExpiryCompleted) diagnostics.log("[INPUT] crossing lane=%u entry=%lu relevant_us=%llu disposition=IGNORED reason=POST_EXPIRY_ENTRY_SETTLED",unsigned(lane),static_cast<unsigned long>(entry->raceEntryId),static_cast<unsigned long long>(event.relevantTime));
    else diagnostics.log("[INPUT] crossing lane=%u entry=%lu relevant_us=%llu disposition=RECEIVED",unsigned(lane),entry?static_cast<unsigned>(entry->raceEntryId):0U,static_cast<unsigned long long>(event.relevantTime));
#else
    diagnostics.log("[INPUT EVENT] device=%08lx capability=%u relevant_us=%llu",
      static_cast<unsigned long>(event.input.device),event.input.capability,
      static_cast<unsigned long long>(event.relevantTime));
#endif
  }
}
void observePresentationState(){
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
  const auto state=browser.current();
  const bool overtimeVisible=state.sessionMode==pp::SessionMode::Endurance&&state.lifecycle!=pp::SessionLifecycle::Ready&&state.finishBehaviour==pp::LapFinishBehaviour::CompleteCurrentLap&&state.overtime>0;
  if(!presentationLogKnown||state.lifecycle!=lastPresentationLifecycle||state.finishBehaviour!=lastPresentationFinish||state.durationExpired!=lastPresentationDurationExpired||overtimeVisible!=lastPresentationOvertimeVisible){
    presentationLogKnown=true;lastPresentationLifecycle=state.lifecycle;lastPresentationFinish=state.finishBehaviour;lastPresentationDurationExpired=state.durationExpired;lastPresentationOvertimeVisible=overtimeVisible;
    diagnostics.log("[PRESENTATION] lifecycle=%s finish=%s duration_expired=%s time_remaining_us=%llu overtime_visible=%s overtime_us=%llu",
      lifecycleName(state.lifecycle),finishName(state.finishBehaviour),state.durationExpired?"true":"false",static_cast<unsigned long long>(state.remainingDuration),overtimeVisible?"true":"false",static_cast<unsigned long long>(state.overtime));
  }
#endif
}
void observeBrowserNetworkEvents(){
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
  pp::BrowserInterface::HttpHealth health{};browser.httpHealth(health);
  const bool wifi=health.wifiConnected,http=health.serverReady;
  if(!networkDiagnosticKnown||wifi!=lastNetworkWifi){diagnostics.log("[WIFI] %s ip=%s reconnects=%lu",wifi?"CONNECTED":"DISCONNECTED",WiFi.localIP().toString().c_str(),static_cast<unsigned long>(health.reconnectAttempts));lastNetworkWifi=wifi;}
  if(!networkDiagnosticKnown||http!=lastNetworkHttp){diagnostics.log("[HTTP] SERVER_%s starts=%lu stops=%lu",http?"STARTED":"STOPPED",static_cast<unsigned long>(health.serverStarts),static_cast<unsigned long>(health.serverStops));lastNetworkHttp=http;}
  if(networkDiagnosticKnown&&health.reconnectAttempts!=lastNetworkReconnects){diagnostics.log("[WIFI] RECONNECT_ATTEMPT count=%lu",static_cast<unsigned long>(health.reconnectAttempts));}
  if(networkDiagnosticKnown&&health.requestErrors!=lastNetworkErrors){diagnostics.log("[HTTP] REQUEST_ERROR count=%lu last_error=%d route=%s",static_cast<unsigned long>(health.requestErrors),health.lastError,health.lastRoute);}
  if(networkDiagnosticKnown&&health.slowRequests!=lastNetworkSlow){diagnostics.log("[HTTP] REQUEST_SLOW count=%lu last_ms=%lu route=%s",static_cast<unsigned long>(health.slowRequests),static_cast<unsigned long>(health.lastDurationMs),health.lastRoute);}
  if(health.lastError!=lastNetworkStartError&&health.lastError){diagnostics.log("[HTTP] REQUEST_ERROR last_error=%d route=%s",health.lastError,health.lastRoute);}
  lastNetworkReconnects=health.reconnectAttempts;lastNetworkErrors=health.requestErrors;lastNetworkSlow=health.slowRequests;lastNetworkStartError=health.lastError;networkDiagnosticKnown=true;
#endif
}
void status(){diagnostics.log("[DEV] %s %s system_us=%llu dropped=%lu; no session, no race",buildIdentity(),
  ready?"IDLE":bootFailed?"FAULT":"STARTING",static_cast<unsigned long long>(systemTime()),static_cast<unsigned long>(diagnostics.dropped));}
const char* lifecycleName(pp::SessionLifecycle value){switch(value){case pp::SessionLifecycle::Ready:return "READY";case pp::SessionLifecycle::Starting:return "STARTING";case pp::SessionLifecycle::Racing:return "RACING";case pp::SessionLifecycle::Paused:return "PAUSED";case pp::SessionLifecycle::Restarting:return "RESTARTING";case pp::SessionLifecycle::Finished:return "FINISHED";default:return "FAULTED";}}
const char* modeName(pp::SessionMode value){switch(value){case pp::SessionMode::None:return "NONE";case pp::SessionMode::OpenPractice:return "OPEN_PRACTICE";case pp::SessionMode::Endurance:return "ENDURANCE";default:return "LAP_RACE";}}
const char* finishName(pp::LapFinishBehaviour value){switch(value){case pp::LapFinishBehaviour::CompleteCurrentLap:return "COMPLETE_CURRENT_LAP";case pp::LapFinishBehaviour::CompleteFullRaceDistance:return "COMPLETE_FULL_RACE_DISTANCE";default:return "IMMEDIATE";}}
const char* operationName(pp::SessionOperation value){switch(value){case pp::SessionOperation::Pause:return "PAUSE";case pp::SessionOperation::HonourRestart:return "HONOUR_RESTART";case pp::SessionOperation::GridRestart:return "GRID_RESTART";case pp::SessionOperation::RaceAgain:return "RACE_AGAIN";case pp::SessionOperation::RestartRace:return "RESTART_RACE";case pp::SessionOperation::EndRace:return "END_RACE";case pp::SessionOperation::Resume:return "RESUME";case pp::SessionOperation::EndSession:return "END_SESSION";case pp::SessionOperation::SkipFinishDisplay:return "SKIP_FINISH_DISPLAY";default:return "RECORDS";}}
const char* rejectionName(pp::RequestRejection value){switch(value){case pp::RequestRejection::None:return "NONE";case pp::RequestRejection::PermissionDenied:return "PERMISSION_DENIED";case pp::RequestRejection::LifecycleNotStartable:return "LIFECYCLE_NOT_STARTABLE";case pp::RequestRejection::InvalidRaceSetup:return "INVALID_RACE_SETUP";case pp::RequestRejection::RequiredCapabilityUnavailable:return "CAPABILITY_UNAVAILABLE";case pp::RequestRejection::SessionDefinitionUnavailable:return "SESSION_DEFINITION_UNAVAILABLE";case pp::RequestRejection::LifecycleNotPausable:return "LIFECYCLE_NOT_PAUSABLE";case pp::RequestRejection::LifecycleNotRestartable:return "LIFECYCLE_NOT_RESTARTABLE";case pp::RequestRejection::PauseSettlementPending:return "PAUSE_SETTLEMENT_PENDING";case pp::RequestRejection::LifecycleNotRaceAgain:return "LIFECYCLE_NOT_RACE_AGAIN";case pp::RequestRejection::ConfirmationRequired:return "CONFIRMATION_REQUIRED";case pp::RequestRejection::LifecycleNotAbandonable:return "LIFECYCLE_NOT_ABANDONABLE";case pp::RequestRejection::StorageUnavailable:return "STORAGE_UNAVAILABLE";case pp::RequestRejection::LifecycleNotResumable:return "LIFECYCLE_NOT_RESUMABLE";default:return "LIFECYCLE_NOT_ENDABLE";}}
uint8_t diagnosticLane(const pp::InputIdentity& input){if(input==pp::InputModule::simulatedDetectorIdentity(0))return 1;if(input==pp::InputModule::simulatedDetectorIdentity(1))return 2;return 0;}
void logLifecycleTransition(){
  const auto state=raceControl.state();
  if(lifecycleLogKnown&&state==lastLoggedLifecycle)return;
  lifecycleLogKnown=true;lastLoggedLifecycle=state;
  const auto* definition=raceControl.definition();
  if(!definition){diagnostics.log("[STATE] lifecycle=%s mode=NONE entries=0",lifecycleName(state));return;}
  diagnostics.log("[STATE] lifecycle=%s mode=%s entries=%u lap_target=%lu duration_min=%u finish=%s",
    lifecycleName(state),modeName(definition->mode()),unsigned(definition->entryCount()),
    static_cast<unsigned long>(definition->lapTarget()),unsigned(definition->durationMinutes()),finishName(definition->finishBehaviour()));
  if(state==pp::SessionLifecycle::Starting){diagnostics.log("[START] GO scheduled_us=%llu reds=%u interval_us=%llu final_delay_us=%llu",
    static_cast<unsigned long long>(raceControl.scheduledGo()),unsigned(raceControl.redLightCount()),
    static_cast<unsigned long long>(definition->redIntervalUs()),static_cast<unsigned long long>(raceControl.finalDelay()));}
  if(state==pp::SessionLifecycle::Paused){diagnostics.log("[STATE] pause_effective_us=%llu remaining_endurance_us=%llu",
    static_cast<unsigned long long>(raceControl.pauseEffectiveAt()),static_cast<unsigned long long>(raceControl.durationRemaining(systemTime())));}
}
void logResourceHealth(){
  pp::BrowserInterface::HttpHealth http{}; browser.httpHealth(http);
  diagnostics.log("[DEV] RESOURCE free_heap=%lu min_heap=%lu max_alloc=%lu loop_worst_us=%llu input_backlog=%lu input_backlog_peak=%lu bus_input=%lu bus_race=%lu bus_presentation=%lu bus_diag=%lu diag_dropped=%lu http_active=%lu http_peak=%lu",
    static_cast<unsigned long>(ESP.getFreeHeap()),static_cast<unsigned long>(ESP.getMinFreeHeap()),static_cast<unsigned long>(ESP.getMaxAllocHeap()),
    static_cast<unsigned long long>(loopWorstUs),static_cast<unsigned long>(input.protectedBacklogDepth()),static_cast<unsigned long>(input.maximumProtectedBacklogDepth()),
    static_cast<unsigned long>(bus.queued(input.endpoint)),static_cast<unsigned long>(bus.queued(raceEngineEndpoint)),static_cast<unsigned long>(bus.queued(presentationEndpoint)),static_cast<unsigned long>(bus.queued(testObserver)),static_cast<unsigned long>(diagnostics.dropped),
    static_cast<unsigned long>(http.activeHandlers),static_cast<unsigned long>(http.peakHandlers));
  diagnostics.log("[DEV] RESOURCE stack_hwm_words=%lu",static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));
}
void browserHealth(){
  pp::BrowserInterface::HttpHealth health;
  browser.httpHealth(health);
  const String ip=WiFi.localIP().toString();
  diagnostics.log("[DEV] HTTP HEALTH wifi_status=%d wifi_connected=%u ip=%s server=%u requests=%lu errors=%lu slow=%lu active=%lu peak=%lu starts=%lu stops=%lu reconnects=%lu last_ms=%lu last_error=%d last_route=%s free_heap=%lu min_heap=%lu max_alloc=%lu loop_worst_us=%llu stack_hwm_words=%lu bus_input=%lu bus_race=%lu bus_presentation=%lu bus_diag=%lu input_backlog=%lu input_peak=%lu diag_dropped=%lu",
    int(WiFi.status()),health.wifiConnected?1U:0U,ip.c_str(),health.serverReady?1U:0U,
    (unsigned long)health.requestCount,(unsigned long)health.requestErrors,(unsigned long)health.slowRequests,
    (unsigned long)health.activeHandlers,(unsigned long)health.peakHandlers,(unsigned long)health.serverStarts,
    (unsigned long)health.serverStops,(unsigned long)health.reconnectAttempts,(unsigned long)health.lastDurationMs,
    health.lastError,health.lastRoute,(unsigned long)ESP.getFreeHeap(),(unsigned long)ESP.getMinFreeHeap(),(unsigned long)ESP.getMaxAllocHeap(),
    static_cast<unsigned long long>(loopWorstUs),static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)),
    (unsigned long)bus.queued(input.endpoint),(unsigned long)bus.queued(raceEngineEndpoint),(unsigned long)bus.queued(presentationEndpoint),(unsigned long)bus.queued(testObserver),
    (unsigned long)input.protectedBacklogDepth(),(unsigned long)input.maximumProtectedBacklogDepth(),(unsigned long)diagnostics.dropped);
  logResourceHealth();
}
void observeDiagnosticEvents(){
  pp::Message event;
  while(bus.receive(testObserver,event)){
    switch(event.type){
      case pp::Type::StartRequest:
        {resetDiagnosticExpiry();const auto* setup=raceControl.proposedRaceSetup();diagnostics.log("[SESSION] start_request mode=%s entries=%u lap_target=%lu duration_min=%u finish=%u relevant_us=%llu",
          modeName(event.sessionMode),unsigned(setup?setup->activeLanes:0),static_cast<unsigned long>(setup?setup->lapTarget:0),unsigned(event.durationMinutes?event.durationMinutes:(setup?setup->durationMinutes:0)),unsigned(event.finishPolicy),static_cast<unsigned long long>(event.relevantTime));}
        break;
      case pp::Type::RequestResult:
        diagnostics.log("[SESSION] request_result correlation=%lu result=%s reason=%s",
          static_cast<unsigned long>(event.correlation),event.requestResult==pp::RequestResult::Accepted?"ACCEPTED":"REJECTED",rejectionName(event.rejection));
        break;
      case pp::Type::SessionOperationRequest:
        diagnostics.log("[SESSION] operation_request op=%s relevant_us=%llu",operationName(event.operation),static_cast<unsigned long long>(event.relevantTime));
        break;
      case pp::Type::SessionOperation:
        diagnostics.log("[STATE] operation op=%s relevant_us=%llu restart=%u",operationName(event.operation),static_cast<unsigned long long>(event.relevantTime),unsigned(event.restartMethod));
        break;
      case pp::Type::GoScheduled:
        diagnostics.log("[START] GO authoritative_scheduled_us=%llu",static_cast<unsigned long long>(event.relevantTime));
        break;
      case pp::Type::LapCompleted:{
        const uint8_t i=raceEngine.entryCount()?raceEngine.entryCount():0; uint32_t laps=0,penalty=0;
        for(uint8_t n=0;n<i;++n)if(raceEngine.entryState(n).raceEntryId==event.raceEntryId){laps=raceEngine.entryState(n).laps;penalty=raceEngine.entryState(n).lapPenalty;break;}
        diagnostics.log("[LAP] accepted entry=%lu lap=%lu factual_laps=%lu lap_us=%llu classified_laps=%lu relevant_us=%llu",
          static_cast<unsigned long>(event.raceEntryId),static_cast<unsigned long>(event.lapNumber),static_cast<unsigned long>(laps),static_cast<unsigned long long>(event.lapTime),static_cast<unsigned long>(laps>penalty?laps-penalty:0),static_cast<unsigned long long>(event.relevantTime));
        break;}
      case pp::Type::Paused:
        diagnostics.log("[STATE] PAUSED fact relevant_us=%llu",static_cast<unsigned long long>(event.relevantTime)); break;
      case pp::Type::RestartScheduled:
        diagnostics.log("[START] restart_scheduled method=%u go_us=%llu",unsigned(event.restartMethod),static_cast<unsigned long long>(event.relevantTime)); break;
      case pp::Type::Resumed:
        diagnostics.log("[START] GO resume_authoritative_us=%llu",static_cast<unsigned long long>(event.relevantTime)); break;
      case pp::Type::FalseStart:
        diagnostics.log("[ERROR] false_start entry=%lu policy=%u relevant_us=%llu",static_cast<unsigned long>(event.raceEntryId),unsigned(event.probe),static_cast<unsigned long long>(event.relevantTime)); break;
      case pp::Type::EnduranceExpired:
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
        diagnostics.log("[ENDURANCE] expiry_relevant_us=%llu finish=%s",static_cast<unsigned long long>(event.relevantTime),finishName(raceControl.definition()?raceControl.definition()->finishBehaviour():pp::LapFinishBehaviour::Immediate));
        for(uint8_t n=0;n<raceEngine.entryCount();++n){const auto&s=raceEngine.entryState(n);const bool eligible=raceControl.definition()&&raceControl.definition()->finishBehaviour()==pp::LapFinishBehaviour::CompleteCurrentLap&&s.timingOriginEstablished&&!s.postExpiryCompleted;diagnosticExpiryEligible[n]=eligible;diagnosticExpirySettled[n]=s.postExpiryCompleted||!eligible;diagnostics.log("[ENDURANCE] eligibility entry=%lu factual_laps=%lu classified_laps=%lu timing_origin=%s eligible=%s settled=%s",static_cast<unsigned long>(s.raceEntryId),static_cast<unsigned long>(s.laps),static_cast<unsigned long>(s.laps>s.lapPenalty?s.laps-s.lapPenalty:0),s.timingOriginEstablished?"YES":"NO",eligible?"YES":"NO",diagnosticExpirySettled[n]?"YES":"NO");}
#else
        diagnostics.log("[ENDURANCE] expiry_relevant_us=%llu",static_cast<unsigned long long>(event.relevantTime));
#endif
        break;
      case pp::Type::CompetitionComplete:
        diagnostics.log("[RESULT] sealed finish_relevant_us=%llu winner_entry=%lu valid=%u integrity=%s",static_cast<unsigned long long>(event.relevantTime),static_cast<unsigned long>(event.raceEntryId),raceEngine.completedResult().valid?1U:0U,raceEngine.faulted()?"FAULTED":"OK"); break;
      case pp::Type::HistoryStored:
        diagnostics.log("[RESULT] history_stored sequence=%lu relevant_us=%llu",static_cast<unsigned long>(event.historySequence),static_cast<unsigned long long>(event.relevantTime)); break;
      case pp::Type::StorageFault:
        diagnostics.log("[ERROR] history_storage_fault relevant_us=%llu",static_cast<unsigned long long>(event.relevantTime)); break;
      case pp::Type::RaceIntegrityFault:
        diagnostics.log("[ERROR] race_integrity_fault reason=%u relevant_us=%llu",unsigned(event.integrityReason),static_cast<unsigned long long>(event.relevantTime)); break;
      case pp::Type::PauseSettled:
        diagnostics.log("[STATE] pause_settled relevant_us=%llu",static_cast<unsigned long long>(event.relevantTime)); break;
      case pp::Type::FinishSettled:
        diagnostics.log("[RESULT] finish_settled relevant_us=%llu",static_cast<unsigned long long>(event.relevantTime)); break;
      default: break;
    }
  }
}
#ifdef PP_ACCEPTANCE
#include "../tests/acceptance_probe.inc"
#endif
#ifdef PP_STAGE2_ACCEPTANCE
#include "../tests/stage2_acceptance_probe.inc"
#endif
#ifdef PP_STAGE3_ACCEPTANCE
#include "../tests/stage3_acceptance_probe.inc"
#endif
#ifdef PP_STAGE4_ACCEPTANCE
#include "../tests/stage4_acceptance_probe.inc"
#endif
#ifdef PP_STAGE5_ACCEPTANCE
#include "../tests/stage5_acceptance_probe.inc"
#endif
#ifdef PP_STAGE6_ACCEPTANCE
#include "../tests/stage6_acceptance_probe.inc"
#endif
#ifdef PP_STAGE6_DEMO
#include "../tests/stage6_demo_probe.inc"
#endif
#ifdef PP_STAGE7_ACCEPTANCE
#include "../tests/stage7_acceptance_probe.inc"
#endif
#ifdef PP_STAGE7_DEMO
#include "../tests/stage7_demo_probe.inc"
#endif
#ifdef PP_STAGE8_ACCEPTANCE
#include "../tests/stage8_acceptance_probe.inc"
#endif
#ifdef PP_STAGE9_ACCEPTANCE
#include "../tests/stage9_acceptance_probe.inc"
#endif
#ifdef PP_STAGE10_ACCEPTANCE
#include "../tests/stage10_acceptance_probe.inc"
#endif
#ifdef PP_STAGE11_ACCEPTANCE
#include "../tests/stage11_acceptance_probe.inc"
#endif
#ifdef PP_STAGE12_ACCEPTANCE
#include "../tests/stage12_acceptance_probe.inc"
#endif
#ifdef PP_STAGE13_ACCEPTANCE
#include "../tests/stage13_acceptance_probe.inc"
#endif
#ifdef PP_STAGE14A_ACCEPTANCE
#include "../tests/stage14a_acceptance_probe.inc"
#endif
#ifdef PP_STAGE14B_ACCEPTANCE
#include "../tests/stage14b_acceptance_probe.inc"
#endif
#ifdef PP_STAGE14C_ACCEPTANCE
#include "../tests/stage14c_acceptance_probe.inc"
#include "../tests/stage14c_facilities_probe.inc"
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
#ifdef PP_STAGE4_ACCEPTANCE
  stage4AcceptanceBeforeBoot();
#endif
#ifdef PP_STAGE5_ACCEPTANCE
  stage5AcceptanceBeforeBoot();
#endif
#ifdef PP_STAGE6_ACCEPTANCE
  stage6AcceptanceBeforeBoot();
#endif
#ifdef PP_STAGE6_DEMO
  stage6DemoBeforeBoot();
#endif
#ifdef PP_STAGE7_ACCEPTANCE
  stage7AcceptanceBeforeBoot();
#endif
 #ifdef PP_STAGE7_DEMO
  stage7DemoBeforeBoot();
#endif
#ifdef PP_STAGE8_ACCEPTANCE
  stage8AcceptanceBeforeBoot();
#endif
#ifdef PP_STAGE9_ACCEPTANCE
  stage9AcceptanceBeforeBoot();
#endif
#ifdef PP_STAGE10_ACCEPTANCE
  stage10AcceptanceBeforeBoot();
#endif
#ifdef PP_STAGE11_ACCEPTANCE
  stage11AcceptanceBeforeBoot();
#endif
#ifdef PP_STAGE12_ACCEPTANCE
  stage12AcceptanceBeforeBoot();
#endif
#ifdef PP_STAGE13_ACCEPTANCE
  stage13AcceptanceBeforeBoot();
#endif
#ifdef PP_STAGE14A_ACCEPTANCE
  stage14aAcceptanceBeforeBoot();
#endif
#ifdef PP_STAGE14B_ACCEPTANCE
  stage14bAcceptanceBeforeBoot();
#endif
#ifdef PP_STAGE14C_ACCEPTANCE
  stage14cAcceptanceBeforeBoot();
#endif
  diagnostics.log("[DEV] %s -- diagnostics are not product State",buildIdentity());
  auto first=systemTime(),second=systemTime();testsPassed=second>=first;
  diagnostics.log("[INIT] System Time %s: monotonic 64-bit microseconds",testsPassed?"READY":"FAIL");
  storage.begin();browserAuthority.begin();
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
  diagnostics.log("[INIT] Development persistence backend READY (versioned NVS/Wokwi boundary)");
#endif
#if defined(PP_STAGE10_DEMO) || defined(PP_STAGE11_DEMO) || defined(PP_STAGE11_ACCEPTANCE) || defined(PP_STAGE12_DEMO) || defined(PP_STAGE12_ACCEPTANCE) || defined(PP_STAGE13_DEMO) || defined(PP_STAGE13_ACCEPTANCE) || defined(PP_STAGE14A_DEMO) || defined(PP_STAGE14A_ACCEPTANCE) || defined(PP_STAGE14B_DEMO) || defined(PP_STAGE14B_ACCEPTANCE) || defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
  browserAuthority.clearForFixture();
#endif
  diagnostics.log("[INIT] Memory %s: NVS behind Memory boundary",storage.available()?"READY":"DEGRADED");
  testsPassed&=bus.subscribe(memoryEndpoint,pp::Type::LoadConfiguration);
  testsPassed&=bus.subscribe(lifecycle,pp::Type::ConfigurationLoaded);
  testsPassed&=bus.subscribe(testPublisher,pp::Type::DiagnosticProbe);
  testsPassed&=bus.subscribe(testObserver,pp::Type::DiagnosticProbe);
  testsPassed&=bus.subscribe(inputObserver,pp::Type::InputEvent);
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
  // Diagnostics observes authoritative event boundaries without participating
  // in control flow. High-frequency NoticeboardChanged and ordinary bus
  // traffic remain intentionally unobserved here.
  for(pp::Type type : {pp::Type::StartRequest,pp::Type::RequestResult,pp::Type::SessionOperationRequest,pp::Type::SessionOperation,pp::Type::GoScheduled,pp::Type::LapCompleted,pp::Type::Paused,pp::Type::RestartScheduled,pp::Type::Resumed,pp::Type::FalseStart,pp::Type::EnduranceExpired,pp::Type::CompetitionComplete,pp::Type::HistoryStored,pp::Type::StorageFault,pp::Type::RaceIntegrityFault,pp::Type::PauseSettled,pp::Type::FinishSettled}) testsPassed&=bus.subscribe(testObserver,type);
#endif
  testsPassed&=bus.subscribe(input.endpoint,pp::Type::SessionOperation);
  testsPassed&=bus.subscribe(raceEngineEndpoint,pp::Type::InputEvent);
  testsPassed&=bus.subscribe(raceEngineEndpoint,pp::Type::GoScheduled);
  testsPassed&=bus.subscribe(raceEngineEndpoint,pp::Type::SessionOperation);
  testsPassed&=bus.subscribe(raceEngineEndpoint,pp::Type::InputSettlement);
  testsPassed&=bus.subscribe(raceEngineEndpoint,pp::Type::FinishSettled);
  testsPassed&=bus.subscribe(raceEngineEndpoint,pp::Type::EnduranceExpired);
  testsPassed&=bus.subscribe(input.endpoint,pp::Type::FinishSettlement);
  testsPassed&=bus.subscribe(raceControlEndpoint,pp::Type::CompetitionComplete);
  testsPassed&=bus.subscribe(raceControlEndpoint,pp::Type::StartRequest);
  testsPassed&=bus.subscribe(raceControlEndpoint,pp::Type::SessionOperationRequest);
  testsPassed&=bus.subscribe(raceControlEndpoint,pp::Type::SetupRequest);
  testsPassed&=bus.subscribe(raceControlEndpoint,pp::Type::PauseSettled);
  testsPassed&=bus.subscribe(raceControlEndpoint,pp::Type::RaceIntegrityFault);
  testsPassed&=bus.subscribe(raceEngineEndpoint,pp::Type::RaceIntegrityFault);
  testsPassed&=bus.subscribe(presentationEndpoint,pp::Type::NoticeboardChanged);
  testsPassed&=bus.subscribe(presentationEndpoint,pp::Type::GoScheduled);
  testsPassed&=bus.subscribe(presentationEndpoint,pp::Type::LapCompleted);
  testsPassed&=bus.subscribe(presentationEndpoint,pp::Type::CompetitionComplete);
  testsPassed&=bus.subscribe(presentationEndpoint,pp::Type::RequestResult);
  testsPassed&=bus.subscribe(presentationEndpoint,pp::Type::Paused);
  testsPassed&=bus.subscribe(presentationEndpoint,pp::Type::RestartScheduled);
  testsPassed&=bus.subscribe(presentationEndpoint,pp::Type::Resumed);
  testsPassed&=bus.subscribe(presentationEndpoint,pp::Type::FalseStart);
  testsPassed&=bus.subscribe(presentationEndpoint,pp::Type::HistoryStored);
  testsPassed&=bus.subscribe(presentationEndpoint,pp::Type::StorageFault);
  testsPassed&=bus.subscribe(presentationEndpoint,pp::Type::EnduranceExpired);
#if defined(PP_STAGE11_DEMO) || defined(PP_STAGE11_ACCEPTANCE) || defined(PP_STAGE12_DEMO) || defined(PP_STAGE12_ACCEPTANCE) || defined(PP_STAGE13_DEMO) || defined(PP_STAGE13_ACCEPTANCE) || defined(PP_STAGE14A_DEMO) || defined(PP_STAGE14A_ACCEPTANCE) || defined(PP_STAGE14B_DEMO) || defined(PP_STAGE14B_ACCEPTANCE) || defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
  browser.setFixtureCallbacks(browserFixturePass,browserFixtureReset);
#endif
  browser.begin();
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
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
  const pp::Time loopStarted=systemTime();
#endif
  memoryModule.tick();
#if defined(PP_STAGE11_DEMO) || defined(PP_STAGE11_ACCEPTANCE) || defined(PP_STAGE12_DEMO) || defined(PP_STAGE12_ACCEPTANCE) || defined(PP_STAGE13_DEMO) || defined(PP_STAGE13_ACCEPTANCE) || defined(PP_STAGE14A_DEMO) || defined(PP_STAGE14A_ACCEPTANCE) || defined(PP_STAGE14B_DEMO) || defined(PP_STAGE14B_ACCEPTANCE) || defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
  const pp::Time fixtureNow=systemTime();
  if(simulatedARelease&&fixtureNow>=simulatedARelease){input.setSimulatedSource(false);simulatedARelease=0;}
  if(simulatedBRelease&&fixtureNow>=simulatedBRelease){input.setSimulatedSourceB(false);simulatedBRelease=0;}
#endif
  // Race Control owns authoritative time boundaries.  Publish an Endurance
  // expiry before sampling a same-loop detector passage so Race Engine sees
  // the expiry marker before classifying any post-expiry input.
  raceControl.tick(systemTime());
  input.tick(systemTime());
  raceEngine.tick();
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
  logLifecycleTransition();
#endif
  browser.tick();
  if(!browserWifiStateKnown||browserWifiState!=browser.wifiConnected()){
    browserWifiStateKnown=true;browserWifiState=browser.wifiConnected();
    if(browserWifiState){
      diagnostics.log("[DEV] Browser WiFi CONNECTED ip=%s reconnect_attempts=%lu",WiFi.localIP().toString().c_str(),static_cast<unsigned long>(browser.wifiReconnectAttempts()));
    }else{
      diagnostics.log("[DEV] Browser WiFi DISCONNECTED reconnect_attempts=%lu",static_cast<unsigned long>(browser.wifiReconnectAttempts()));
    }
  }
  if(!browserHttpStateKnown||browserHttpState!=browser.serverReady()){
    browserHttpStateKnown=true;browserHttpState=browser.serverReady();
    diagnostics.log("[DEV] Browser HTTP server %s",browserHttpState?"STARTED":"STOPPED");
  }
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
  observeBrowserNetworkEvents();
  observePresentationState();
#endif
#ifdef PP_STAGE10_DEMO
  if(!stage10ServerReported&&(browser.serverReady()||browser.serverStartError())){
    stage10ServerReported=true;
    diagnostics.log("[DEV] Stage10 Browser server=%s error=%d",browser.serverReady()?"READY":"FAILED",browser.serverStartError());
  }
#endif
#ifdef PP_STAGE11_DEMO
  if(!stage10ServerReported&&(browser.serverReady()||browser.serverStartError())){
    stage10ServerReported=true;
    diagnostics.log("[DEV] Stage11 Browser server=%s error=%d",browser.serverReady()?"READY":"FAILED",browser.serverStartError());
  }
#endif
#if defined(PP_STAGE12_DEMO) || defined(PP_STAGE13_DEMO) || defined(PP_STAGE14A_DEMO) || defined(PP_STAGE14B_DEMO) || defined(PP_STAGE14C_DEMO)
  if(!stage10ServerReported&&(browser.serverReady()||browser.serverStartError())){
    stage10ServerReported=true;
    diagnostics.log("[DEV] %s Browser server=%s error=%d",buildIdentity(),browser.serverReady()?"READY":"FAILED",browser.serverStartError());
  }
#endif
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
  observeDiagnosticEvents();
#endif
  observeInputEvents();
  pp::Message message;
  if(!ready&&!bootFailed&&bus.receive(lifecycle,message)){
    if(message.type!=pp::Type::ConfigurationLoaded||message.correlation!=1||!pp::valid(message.configuration)){
      bootFailed=true;diagnostics.log("STAGE1_FAIL configuration response");
    }else{
      workingConfiguration=message.configuration;
#if defined(PP_STAGE9_DEMO) || defined(PP_STAGE10_DEMO) || defined(PP_STAGE11_DEMO) || defined(PP_STAGE12_DEMO) || defined(PP_STAGE13_DEMO) || defined(PP_STAGE14A_DEMO) || defined(PP_STAGE14B_DEMO) || defined(PP_STAGE14C_DEMO)
      proposedRaceSetup.startFinish={pp::InputModule::simulatedDetectorIdentity(),1,pp::InputRole::StartFinish};
      proposedRaceSetup.secondStartFinish={pp::InputModule::simulatedDetectorBIdentity(),2,pp::InputRole::StartFinish};
      proposedRaceSetup.selectedMugId=91;proposedRaceSetup.secondMugId=92;proposedRaceSetup.activeLanes=2;
#if defined(PP_STAGE13_DEMO)
      // Stage 13's human fixture starts as an explicit three-lap race so the
      // visible target and the first checkpoint scenario cannot be confused
      // with the shorter Stage 11/12 demo fixture.
      proposedRaceSetup.lapTarget=3;
#else
      proposedRaceSetup.lapTarget=2;
#endif
#else
      proposedRaceSetup.lapTarget=workingConfiguration.laps;
#endif
      raceControl.setProposedRaceSetup(proposedRaceSetup);
      raceControl.setRequiredCapabilityAvailable(true);
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
      diagnostics.log("%s",ready?"STAGE4_PASS Session Definition scaffold ready; no race behaviour":"STAGE4_FAIL startup");
      diagnostics.log("[DEV] Serial: ? status, h HTTP/WiFi health, t bus self-test, q quiet, v diagnostics on");
      nextStatus=systemTime()+10000000;
    }
  }
  // Bounded serial command work. 0/1 control detector A and 2/3 control
  // detector B. These are source-facing simulated detector controls, not P&P
  // Requests or bus events.
  for(unsigned i=0;i<8&&Serial.available();++i){
    const char c=Serial.read();
    if(c=='q')diagnostics.enable(false);
    else if(c=='v'){diagnostics.enable(true);status();}
    else if(c=='?')status();
    else if(c=='h')browserHealth();
#if defined(PP_STAGE11_DEMO) || defined(PP_STAGE11_ACCEPTANCE) || defined(PP_STAGE12_DEMO) || defined(PP_STAGE12_ACCEPTANCE) || defined(PP_STAGE13_DEMO) || defined(PP_STAGE13_ACCEPTANCE) || defined(PP_STAGE14A_DEMO) || defined(PP_STAGE14A_ACCEPTANCE) || defined(PP_STAGE14B_DEMO) || defined(PP_STAGE14B_ACCEPTANCE) || defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
    else if(c=='x'){activeSession.clearForFixture();raceControl.resetForFixture();raceEngine.resetForFixture();browser.clearForFixture();browserAuthority.clearForFixture();diagnostics.log("[DEV] TEST RESET READY");}
#endif
    else if(c=='t'){const bool ok=pp::busSelfTest(bus,testPublisher,testObserver,input.endpoint,report);diagnostics.log("[BUS SELF-TEST] %s",ok?"PASS":"FAIL");}
    else if(c=='0'){input.setSimulatedSource(false);
#if defined(PP_STAGE11_DEMO) || defined(PP_STAGE11_ACCEPTANCE) || defined(PP_STAGE12_DEMO) || defined(PP_STAGE12_ACCEPTANCE) || defined(PP_STAGE13_DEMO) || defined(PP_STAGE13_ACCEPTANCE) || defined(PP_STAGE14B_DEMO) || defined(PP_STAGE14B_ACCEPTANCE) || defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
      simulatedARelease=0;
#endif
      diagnostics.log("[INPUT SOURCE] simulated detector INACTIVE");}
    else if(c=='1'){input.setSimulatedSource(true);
#if defined(PP_STAGE11_DEMO) || defined(PP_STAGE11_ACCEPTANCE) || defined(PP_STAGE12_DEMO) || defined(PP_STAGE12_ACCEPTANCE) || defined(PP_STAGE13_DEMO) || defined(PP_STAGE13_ACCEPTANCE) || defined(PP_STAGE14B_DEMO) || defined(PP_STAGE14B_ACCEPTANCE) || defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
      simulatedARelease=systemTime()+150000;
#endif
      diagnostics.log("[INPUT SOURCE] simulated detector PASSAGE ACTIVE");}
    else if(c=='2'){input.setSimulatedSourceB(true);
#if defined(PP_STAGE11_DEMO) || defined(PP_STAGE11_ACCEPTANCE) || defined(PP_STAGE12_DEMO) || defined(PP_STAGE12_ACCEPTANCE) || defined(PP_STAGE13_DEMO) || defined(PP_STAGE13_ACCEPTANCE) || defined(PP_STAGE14B_DEMO) || defined(PP_STAGE14B_ACCEPTANCE)
      simulatedBRelease=systemTime()+150000;
#endif
      diagnostics.log("[INPUT SOURCE] simulated detector B PASSAGE ACTIVE");}
    else if(c=='3'){input.setSimulatedSourceB(false);
#if defined(PP_STAGE11_DEMO) || defined(PP_STAGE11_ACCEPTANCE) || defined(PP_STAGE12_DEMO) || defined(PP_STAGE12_ACCEPTANCE) || defined(PP_STAGE13_DEMO) || defined(PP_STAGE13_ACCEPTANCE)
      simulatedBRelease=0;
#endif
      diagnostics.log("[INPUT SOURCE] simulated detector B INACTIVE");}
#ifdef PP_WOKWI_DEV
    // This resets the simulated chip only. It is unavailable in normal, demo,
    // and acceptance builds, and avoids the web project's Restart action which
    // replaces a temporary custom-firmware upload with its cloud project image.
    else if(c=='r'){Serial.flush();ESP.restart();}
#endif
#ifdef PP_ACCEPTANCE
    else acceptanceCommand(c);
#endif
#ifdef PP_STAGE2_ACCEPTANCE
    else stage2AcceptanceCommand(c);
#endif
#ifdef PP_STAGE3_ACCEPTANCE
    else stage3AcceptanceCommand(c);
#endif
#ifdef PP_STAGE4_ACCEPTANCE
    else stage4AcceptanceCommand(c);
#endif
#ifdef PP_STAGE5_ACCEPTANCE
    else stage5AcceptanceCommand(c);
#endif
#ifdef PP_STAGE6_ACCEPTANCE
    else stage6AcceptanceCommand(c);
#endif
#ifdef PP_STAGE6_DEMO
    else stage6DemoCommand(c);
#endif
#ifdef PP_STAGE7_ACCEPTANCE
    else stage7AcceptanceCommand(c);
#endif
#ifdef PP_STAGE7_DEMO
    else stage7DemoCommand(c);
#endif
#ifdef PP_STAGE8_ACCEPTANCE
    else stage8AcceptanceCommand(c);
#endif
#ifdef PP_STAGE9_ACCEPTANCE
    else stage9AcceptanceCommand(c);
#endif
#ifdef PP_STAGE10_ACCEPTANCE
    else stage10AcceptanceCommand(c);
#endif
#ifdef PP_STAGE11_ACCEPTANCE
    else stage11AcceptanceCommand(c);
#endif
#ifdef PP_STAGE12_ACCEPTANCE
    else stage12AcceptanceCommand(c);
#endif
#ifdef PP_STAGE13_ACCEPTANCE
    else stage13AcceptanceCommand(c);
#endif
#ifdef PP_STAGE14A_ACCEPTANCE
    else stage14aAcceptanceCommand(c);
#endif
#ifdef PP_STAGE14B_ACCEPTANCE
    else stage14bAcceptanceCommand(c);
#endif
#ifdef PP_STAGE14C_ACCEPTANCE
    else stage14cAcceptanceCommand(c);
#endif
  }
#if !defined(PP_STAGE14C_DEMO) && !defined(PP_STAGE14C_ACCEPTANCE)
  if(ready&&systemTime()>=nextStatus){status();nextStatus=systemTime()+10000000;}
#endif
#if defined(PP_STAGE14C_DEMO) || defined(PP_STAGE14C_ACCEPTANCE)
  const pp::Time loopElapsed=systemTime()-loopStarted;if(loopElapsed>loopWorstUs)loopWorstUs=loopElapsed;
#endif
  diagnostics.flush();
#ifdef PP_STAGE11_ACCEPTANCE
  stage11Output.flush();
#endif
  delay(1);
#ifdef PP_ACCEPTANCE
  acceptanceTick();
#endif
#ifdef PP_STAGE2_ACCEPTANCE
  stage2AcceptanceTick();
#endif
#ifdef PP_STAGE3_ACCEPTANCE
  stage3AcceptanceTick();
#endif
#ifdef PP_STAGE4_ACCEPTANCE
  stage4AcceptanceTick();
#endif
#ifdef PP_STAGE5_ACCEPTANCE
  stage5AcceptanceTick();
#endif
#ifdef PP_STAGE6_ACCEPTANCE
  stage6AcceptanceTick();
#endif
#ifdef PP_STAGE6_DEMO
  stage6DemoTick();
#endif
#ifdef PP_STAGE7_ACCEPTANCE
  stage7AcceptanceTick();
#endif
#ifdef PP_STAGE7_DEMO
  stage7DemoTick();
#endif
#ifdef PP_STAGE8_ACCEPTANCE
  stage8AcceptanceTick();
#endif
#ifdef PP_STAGE9_ACCEPTANCE
  stage9AcceptanceTick();
#endif
#ifdef PP_STAGE10_ACCEPTANCE
  stage10AcceptanceTick();
#endif
#ifdef PP_STAGE11_ACCEPTANCE
  stage11AcceptanceTick();
#endif
#ifdef PP_STAGE12_ACCEPTANCE
  stage12AcceptanceTick();
#endif
#ifdef PP_STAGE13_ACCEPTANCE
  stage13AcceptanceTick();
#endif
#ifdef PP_STAGE14A_ACCEPTANCE
  stage14aAcceptanceTick();
#endif
#ifdef PP_STAGE14B_ACCEPTANCE
  stage14bAcceptanceTick();
#endif
#ifdef PP_STAGE14C_ACCEPTANCE
  stage14cAcceptanceTick();
#endif
#ifdef PP_VERIFY
  if(verificationReboot&&systemTime()>=rebootAt)ESP.restart();
#endif
}
