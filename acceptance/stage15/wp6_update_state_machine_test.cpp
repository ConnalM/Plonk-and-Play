#include "pp/firmware_update.h"

#include <iostream>

using namespace pp;

static FirmwareCandidate validCandidate(uint32_t version = 11) {
  FirmwareCandidate c{};
  c.identity = {"plonk-and-play", "esp32-8048s070", "candidate-11", version};
  c.imageSize = 123456;
  c.complete = true;
  c.integrityValid = true;
  c.authenticated = true;
  return c;
}

static FirmwareUpdateRequestContext masterReady(bool supportDowngrade = false) {
  return {true, true, supportDowngrade};
}

static int fail(const char* name) {
  std::cerr << "FAIL " << name << "\n";
  return 1;
}

int main() {
  DefaultFirmwareValidationProvider validator("plonk-and-play", "esp32-8048s070");
  FirmwareIdentity known{"plonk-and-play", "esp32-8048s070", "known-10", 10};

  {
    FirmwareUpdateStateMachine m(known, validator);
    if (m.beginRequest({false, true, false}) || m.lastRejection() != FirmwareUpdateRejection::PermissionDenied) return fail("unauthorised");
    if (m.beginRequest({true, false, false}) || m.lastRejection() != FirmwareUpdateRejection::LifecycleNotSettled) return fail("lifecycle");
  }

  const auto checkValidation = [&](FirmwareCandidate candidate, FirmwareValidationFailure expected, const char* name) {
    FirmwareUpdateStateMachine m(known, validator);
    if (!m.beginRequest(masterReady()) || m.finishStaging(candidate) || m.state() != FirmwareUpdateState::Failed || m.validationFailure() != expected) return fail(name);
    if (m.knownGood().version != 10 || m.customerDataTouched()) return fail("failed-path-retention");
    return 0;
  };
  auto invalid = validCandidate(); invalid.authenticated = false;
  if (checkValidation(invalid, FirmwareValidationFailure::Unauthenticated, "invalid-auth")) return 1;
  invalid = validCandidate(); invalid.identity.board = "other-board";
  if (checkValidation(invalid, FirmwareValidationFailure::IncompatibleTarget, "incompatible")) return 1;
  invalid = validCandidate(); invalid.complete = false;
  if (checkValidation(invalid, FirmwareValidationFailure::Incomplete, "truncated")) return 1;
  invalid = validCandidate(); invalid.integrityValid = false;
  if (checkValidation(invalid, FirmwareValidationFailure::IntegrityFailure, "corrupt")) return 1;
  invalid = validCandidate(9);
  if (checkValidation(invalid, FirmwareValidationFailure::DowngradeRejected, "downgrade")) return 1;

  {
    FirmwareUpdateStateMachine m(known, validator);
    if (!m.beginRequest(masterReady()) || !m.finishStaging(validCandidate())) return fail("duplicate-staging-setup");
    if (m.finishStaging(validCandidate()) || m.lastRejection() != FirmwareUpdateRejection::InvalidCandidateState) return fail("duplicate-staging");
  }

  {
    FirmwareUpdateStateMachine m(known, validator);
    if (!m.beginRequest(masterReady()) || !m.finishStaging(validCandidate()) || m.state() != FirmwareUpdateState::CandidateReady) return fail("candidate-ready");
    if (!m.beginInstall() || m.state() != FirmwareUpdateState::AwaitingHealthConfirmation) return fail("pending-health");
    if (!m.healthConfirmation(true) || m.state() != FirmwareUpdateState::Committed || m.knownGood().version != 11) return fail("commit");
    if (m.customerDataTouched()) return fail("commit-data");
  }

  {
    FirmwareUpdateStateMachine m(known, validator);
    if (!m.beginRequest(masterReady()) || !m.finishStaging(validCandidate()) || !m.beginInstall()) return fail("rollback-setup");
    if (m.healthConfirmation(false) || m.state() != FirmwareUpdateState::RollingBack || m.knownGood().version != 10) return fail("rollback");
    if (m.completeRollback(false) || m.state() != FirmwareUpdateState::RecoveryRequired) return fail("rollback-failure");
    // RecoveryRequired is deliberately retryable only through a new explicit request.
    if (!m.beginRequest(masterReady()) || m.state() != FirmwareUpdateState::ReceivingStaging) return fail("recovery-retry");
    if (m.customerDataTouched()) return fail("rollback-data");
  }

  {
    FirmwareUpdateStateMachine m(known, validator);
    if (!m.beginRequest(masterReady()) || !m.finishStaging(validCandidate()) || !m.beginInstall()) return fail("successful-rollback-setup");
    if (m.healthConfirmation(false) || m.state() != FirmwareUpdateState::RollingBack) return fail("candidate-health-failure");
    if (!m.completeRollback(true) || m.state() != FirmwareUpdateState::KnownGoodRestored) return fail("successful-rollback");
    if (m.knownGood().version != 10 || m.customerDataTouched()) return fail("successful-rollback-retention");
    if (!m.beginRequest(masterReady()) || m.state() != FirmwareUpdateState::ReceivingStaging) return fail("post-rollback-update");
  }

  std::cout << "WP6_UPDATE_STATE_MACHINE_PASS\n";
  return 0;
}
