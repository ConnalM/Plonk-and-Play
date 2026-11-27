#include "pp/s4_production_role_guard.h"

#if !defined(PP_S4_PRODUCTION_CORE)
#error "core_runtime_esp32.cpp must only be compiled as the Core role"
#endif

#include <Arduino.h>
#include <Preferences.h>

#include "pp/browser_interface.h"
#include "pp/core_runtime_contract.h"
#include "pp/core_runtime_wiring.h"
#include "pp/noticeboard.h"
#include "pp/persistent_storage.h"
#include "pp/race_control.h"
#include "pp/race_engine.h"
#include "pp/system_time.h"
#include "pp/track_configuration.h"

namespace {

class CoreNvsStore final : public pp::Store {
 public:
  void begin() { ready_ = preferences_.begin("pp-core", false); }
  bool available() const override { return ready_; }
  bool read(unsigned slot, uint8_t* data, size_t size) override {
    if (!ready_ || !data) return false;
    char key[16]{};
    keyFor(slot, key, sizeof(key));
    if (!preferences_.isKey(key)) return false;
    const size_t stored = preferences_.getBytesLength(key);
    if (stored != size) {
      memset(data, 0, size);
      return true;
    }
    return preferences_.getBytes(key, data, size) == size;
  }
  bool write(unsigned slot, const uint8_t* data, size_t size) override {
    if (!ready_ || !data) return false;
    char key[16]{};
    keyFor(slot, key, sizeof(key));
    return preferences_.putBytes(key, data, size) == size;
  }
  bool clear() override { return ready_ && preferences_.clear(); }

 private:
  static void keyFor(unsigned slot, char* key, size_t size) {
    snprintf(key, size, "core-%u", slot);
  }

  Preferences preferences_;
  bool ready_ = false;
};

class CoreAuthorityStore final : public pp::AuthorityStore {
 public:
  void begin() { ready_ = preferences_.begin("pp-core-auth", false); }
  bool loadMaster(uint64_t& fingerprint) override {
    if (!ready_ || !preferences_.isKey("master")) return false;
    fingerprint = preferences_.getULong64("master", 0);
    return fingerprint != 0;
  }
  bool saveMaster(uint64_t fingerprint) override {
    return ready_ && fingerprint != 0 &&
           preferences_.putULong64("master", fingerprint) == sizeof(fingerprint);
  }

 private:
  Preferences preferences_;
  bool ready_ = false;
};

// Storage destructive maintenance is deliberately unavailable until the SD
// owner and recovery path are wired to a reviewed Core board profile.
class CoreBenchMaintenance final : public pp::MaintenanceEndpoint {
 public:
  pp::MaintenanceApiResult restart(uint64_t,
                                   const pp::MaintenanceRequestContext&) override {
    return pp::MaintenanceApiResult::Failed;
  }
  pp::MaintenanceApiResult resetSettings(
      uint64_t, const pp::MaintenanceRequestContext&) override {
    return pp::MaintenanceApiResult::Failed;
  }
  pp::MaintenanceApiResult eraseEverything(
      uint64_t, const pp::MaintenanceRequestContext&) override {
    return pp::MaintenanceApiResult::Failed;
  }
  bool consumeRebootRequest() override { return false; }
};

class CoreRuntime {
 public:
  CoreRuntime()
      : lifecycle_(bus_.attach(pp::Role::Lifecycle)),
        memoryEndpoint_(bus_.attach(pp::Role::Memory)),
        inputEndpoint_(bus_.attach(pp::Role::Input)),
        raceControlEndpoint_(bus_.attach(pp::Role::RaceControl)),
        raceEngineEndpoint_(bus_.attach(pp::Role::RaceEngine)),
        presentationEndpoint_(bus_.attach(pp::Role::Presentation)),
        memory_(storage_),
        trackConfiguration_(storage_),
        input_(bus_, inputEndpoint_, detectors_),
        raceControl_(bus_, raceControlEndpoint_, activeSession_, &history_, &records_,
                     &detectors_, &trackConfiguration_, &memory_),
        raceEngine_(bus_, raceEngineEndpoint_, activeSession_, &history_, &records_),
        noticeboard_(raceControl_, raceEngine_, activeSession_),
        browser_(bus_, presentationEndpoint_, noticeboard_, &authority_, &history_,
                 &records_, &raceEngine_, nullptr, nullptr, &maintenance_,
                 &detectors_),
        memoryModule_(bus_, memoryEndpoint_, memory_) {}

  void begin() {
    Serial.begin(115200);
    storage_.begin();
    authority_.begin();
    (void)trackConfiguration_.load(track_);
    browser_.restorePersistedAuthority();
    raceControl_.setControllerStartAuthority(presentationEndpoint_, browser_);
    if (!subscribe()) {
      Serial.println("[CORE] runtime wiring rejected; Browser remains offline");
      return;
    }
    configureProposal(pp::Configuration{});
    // The detector and record boundaries are consciously unavailable.  This
    // is an explicit safety state, not a simulated or default-ready track.
    raceControl_.setRequiredCapabilityAvailable(false);
    pp::Message load{};
    load.type = pp::Type::LoadConfiguration;
    load.correlation = 1;
    if (bus_.publish(lifecycle_, load) != pp::Delivery::Delivered) {
      Serial.println("[CORE] configuration load route unavailable; Browser remains offline");
      return;
    }
    browser_.begin();
    initialized_ = true;
  }

  void tick() {
    if (!initialized_) return;
    memoryModule_.tick();
    receiveConfiguration();
    raceControl_.setRequiredCapabilityAvailable(false);
    raceControl_.tick(pp::systemTime());
    input_.tick(pp::systemTime());
    raceEngine_.tick();
    browser_.tick();
  }

 private:
  bool subscribe() {
    return pp::core_runtime::subscribeRuntime(
        bus_, {lifecycle_, memoryEndpoint_, inputEndpoint_, raceControlEndpoint_,
               raceEngineEndpoint_, presentationEndpoint_});
  }

  void receiveConfiguration() {
    pp::Message message;
    while (bus_.receive(lifecycle_, message)) {
      if (message.type != pp::Type::ConfigurationLoaded || message.correlation != 1) continue;
      configureProposal(pp::valid(message.configuration) ? message.configuration
                                                          : pp::Configuration{});
    }
  }

  void configureProposal(const pp::Configuration& configuration) {
    pp::ProposedRaceSetup setup{};
    setup.activeLanes = 2;
    setup.mode = configuration.mode == static_cast<uint8_t>(pp::SessionMode::Endurance)
                     ? pp::SessionMode::Endurance
                     : pp::SessionMode::LapRace;
    setup.lapTarget = configuration.laps ? configuration.laps : 10;
    setup.durationMinutes = configuration.durationMinutes ? configuration.durationMinutes : 10;
    setup.finish = setup.mode == pp::SessionMode::Endurance
                     ? pp::LapFinishBehaviour::CompleteCurrentLap
                     : pp::LapFinishBehaviour::CompleteFullRaceDistance;
    setup.entries[0].startFinish = {pp::core_runtime::UnavailableDetectorProvider::laneIdentity(1),
                                    1, pp::InputRole::StartFinish};
    setup.entries[1].startFinish = {pp::core_runtime::UnavailableDetectorProvider::laneIdentity(2),
                                    2, pp::InputRole::StartFinish};
    setup.entries[0].mugId = configuration.lane1MugId ? configuration.lane1MugId : 1;
    setup.entries[1].mugId = configuration.lane2MugId ? configuration.lane2MugId : 2;
    setup.startFinish = setup.entries[0].startFinish;
    setup.secondStartFinish = setup.entries[1].startFinish;
    setup.selectedMugId = setup.entries[0].mugId;
    setup.secondMugId = setup.entries[1].mugId;
    raceControl_.setProposedRaceSetup(setup);
  }

  pp::Bus bus_;
  pp::Bus::Endpoint lifecycle_;
  pp::Bus::Endpoint memoryEndpoint_;
  pp::Bus::Endpoint inputEndpoint_;
  pp::Bus::Endpoint raceControlEndpoint_;
  pp::Bus::Endpoint raceEngineEndpoint_;
  pp::Bus::Endpoint presentationEndpoint_;
  CoreNvsStore storage_;
  CoreAuthorityStore authority_;
  pp::Memory memory_;
  pp::TrackConfigurationMemory trackConfiguration_;
  pp::TrackConfiguration track_{};
  pp::core_runtime::UnavailableDetectorProvider detectors_;
  pp::core_runtime::UnavailableHistoryStore history_;
  pp::core_runtime::UnavailableTrackRecordStore records_;
  pp::ActiveSessionDefinition activeSession_;
  pp::InputModule input_;
  pp::RaceControlModule raceControl_;
  pp::RaceEngineModule raceEngine_;
  pp::Noticeboard noticeboard_;
  CoreBenchMaintenance maintenance_;
  pp::BrowserInterface browser_;
  pp::MemoryModule memoryModule_;
  bool initialized_ = false;
};

CoreRuntime runtime;

}  // namespace

void setup() { runtime.begin(); }
void loop() { runtime.tick(); }
