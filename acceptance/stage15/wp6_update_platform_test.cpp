#include "pp/firmware_update.h"

#include <iostream>

using namespace pp;

// Deterministic stand-in for the ESP-IDF OTA boundary.  It exercises the
// contract (inactive-slot staging, pending boot, health commit and rollback)
// without pretending that host tests are bootloader tests.
class FakeOtaPlatform {
 public:
  explicit FakeOtaPlatform(FirmwareUpdateStateMachine& machine) : machine_(machine) {}

  bool stageAndSelect(const FirmwareCandidate& candidate) {
    if (activeSlot_ == candidateSlot_) return false;
    if (!machine_.finishStaging(candidate)) return false;
    ++stageWrites_;
    if (!machine_.beginInstall()) return false;
    pending_ = true;
    return true;
  }

  bool confirm(bool healthy, bool rollbackWorks = true) {
    if (!pending_) return false;
    if (healthy) {
      if (!machine_.healthConfirmation(true)) return false;
      activeSlot_ = candidateSlot_;
      pending_ = false;
      return true;
    }
    machine_.healthConfirmation(false);
    machine_.completeRollback(rollbackWorks);
    pending_ = false;
    if (rollbackWorks) activeSlot_ = knownGoodSlot_;
    return rollbackWorks;
  }

  unsigned stageWrites() const { return stageWrites_; }
  unsigned activeSlot() const { return activeSlot_; }

 private:
  FirmwareUpdateStateMachine& machine_;
  unsigned knownGoodSlot_ = 0;
  unsigned activeSlot_ = 0;
  unsigned candidateSlot_ = 1;
  unsigned stageWrites_ = 0;
  bool pending_ = false;
};

static FirmwareCandidate candidate(uint32_t version) {
  FirmwareCandidate c{};
  c.identity = {"plonk-and-play", "esp32-8048s070", "candidate", version};
  c.imageSize = 100;
  c.complete = true;
  c.integrityValid = true;
  c.authenticated = true;
  return c;
}

static int fail(const char* name) {
  std::cerr << "FAIL " << name << "\n";
  return 1;
}

int main() {
  DefaultFirmwareValidationProvider validator("plonk-and-play", "esp32-8048s070");
  FirmwareIdentity known{"plonk-and-play", "esp32-8048s070", "known", 10};

  {
    FirmwareUpdateStateMachine machine(known, validator);
    FakeOtaPlatform ota(machine);
    if (!machine.beginRequest({true, true, false}) || !ota.stageAndSelect(candidate(11))) return fail("stage-select");
    if (!ota.confirm(true) || machine.state() != FirmwareUpdateState::Committed || ota.activeSlot() != 1) return fail("commit");
    if (ota.stageWrites() != 1 || machine.customerDataTouched()) return fail("commit-boundary");
  }

  {
    FirmwareUpdateStateMachine machine(known, validator);
    FakeOtaPlatform ota(machine);
    if (!machine.beginRequest({true, true, false}) || !ota.stageAndSelect(candidate(11))) return fail("rollback-stage");
    if (!ota.confirm(false, true) || machine.state() != FirmwareUpdateState::KnownGoodRestored || ota.activeSlot() != 0) return fail("rollback-success");
    if (machine.knownGood().version != 10 || machine.customerDataTouched()) return fail("rollback-retention");
  }

  {
    FirmwareUpdateStateMachine machine(known, validator);
    FakeOtaPlatform ota(machine);
    if (!machine.beginRequest({true, true, false}) || !ota.stageAndSelect(candidate(11))) return fail("recovery-stage");
    if (ota.confirm(false, false) || machine.state() != FirmwareUpdateState::RecoveryRequired) return fail("recovery-required");
    if (machine.knownGood().version != 10 || machine.customerDataTouched()) return fail("recovery-retention");
  }

  std::cout << "WP6_UPDATE_PLATFORM_PASS\n";
  return 0;
}
