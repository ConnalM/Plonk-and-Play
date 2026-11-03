#include "pp/firmware_update.h"

#include <iostream>

using namespace pp;

static int fail(const char* name) {
  std::cerr << "FAIL " << name << "\n";
  return 1;
}

static int checkEnvelope(SignedEnvelopeResult result,
                         FirmwareValidationDiagnosticReason reason,
                         FirmwareValidationFailure failure,
                         const char* name) {
  const FirmwareValidationDiagnostic value{
      FirmwareValidationDiagnosticStage::Ppu1Verifier,
      firmwareValidationDiagnosticReason(result),
      static_cast<int32_t>(result),
      firmwareValidationFailure(result)};
  if (value.reason != reason || value.validationFailure != failure || value.code != static_cast<int32_t>(result)) {
    return fail(name);
  }
  return 0;
}

int main() {
  if (checkEnvelope(SignedEnvelopeResult::UnsupportedVersion,
                    FirmwareValidationDiagnosticReason::UnsupportedVersion,
                    FirmwareValidationFailure::IntegrityFailure, "ppu1-unsupported")) return 1;
  if (checkEnvelope(SignedEnvelopeResult::Malformed,
                    FirmwareValidationDiagnosticReason::Malformed,
                    FirmwareValidationFailure::IntegrityFailure, "ppu1-malformed")) return 1;
  if (checkEnvelope(SignedEnvelopeResult::Truncated,
                    FirmwareValidationDiagnosticReason::Truncated,
                    FirmwareValidationFailure::IntegrityFailure, "ppu1-truncated")) return 1;
  if (checkEnvelope(SignedEnvelopeResult::SizeMismatch,
                    FirmwareValidationDiagnosticReason::SizeMismatch,
                    FirmwareValidationFailure::IntegrityFailure, "ppu1-size")) return 1;
  if (checkEnvelope(SignedEnvelopeResult::ProductMismatch,
                    FirmwareValidationDiagnosticReason::ProductMismatch,
                    FirmwareValidationFailure::IncompatibleTarget, "ppu1-product")) return 1;
  if (checkEnvelope(SignedEnvelopeResult::BoardMismatch,
                    FirmwareValidationDiagnosticReason::BoardMismatch,
                    FirmwareValidationFailure::IncompatibleTarget, "ppu1-board")) return 1;
  if (checkEnvelope(SignedEnvelopeResult::DigestMismatch,
                    FirmwareValidationDiagnosticReason::DigestMismatch,
                    FirmwareValidationFailure::IntegrityFailure, "ppu1-digest")) return 1;
  if (checkEnvelope(SignedEnvelopeResult::Unsigned,
                    FirmwareValidationDiagnosticReason::Unsigned,
                    FirmwareValidationFailure::Unauthenticated, "ppu1-unsigned")) return 1;
  if (checkEnvelope(SignedEnvelopeResult::InvalidSignature,
                    FirmwareValidationDiagnosticReason::InvalidSignature,
                    FirmwareValidationFailure::Unauthenticated, "ppu1-signature")) return 1;
  if (checkEnvelope(SignedEnvelopeResult::VerificationUnavailable,
                    FirmwareValidationDiagnosticReason::VerificationUnavailable,
                    FirmwareValidationFailure::IntegrityFailure, "ppu1-verification-unavailable")) return 1;

  FirmwareValidationDiagnostic first{};
  const FirmwareValidationDiagnostic ppu1{
      FirmwareValidationDiagnosticStage::Ppu1Verifier,
      FirmwareValidationDiagnosticReason::InvalidSignature,
      static_cast<int32_t>(SignedEnvelopeResult::InvalidSignature),
      FirmwareValidationFailure::Unauthenticated};
  const FirmwareValidationDiagnostic otaEnd{
      FirmwareValidationDiagnosticStage::EspOtaEnd,
      FirmwareValidationDiagnosticReason::OtaEndFailed,
      -0x1234,
      FirmwareValidationFailure::IntegrityFailure};
  if (!recordFirstFirmwareValidationDiagnostic(first, ppu1) ||
      recordFirstFirmwareValidationDiagnostic(first, otaEnd) || first.stage != FirmwareValidationDiagnosticStage::Ppu1Verifier ||
      first.reason != FirmwareValidationDiagnosticReason::InvalidSignature || first.code != ppu1.code) {
    return fail("first-diagnostic-wins");
  }

  const FirmwareValidationDiagnostic description{
      FirmwareValidationDiagnosticStage::PartitionDescription,
      FirmwareValidationDiagnosticReason::PartitionDescriptionFailed,
      -5,
      FirmwareValidationFailure::IntegrityFailure};
  const FirmwareValidationDiagnostic identity{
      FirmwareValidationDiagnosticStage::ImageIdentity,
      FirmwareValidationDiagnosticReason::IdentityMismatch,
      0,
      FirmwareValidationFailure::IncompatibleTarget};
  const FirmwareValidationDiagnostic stateMachine{
      FirmwareValidationDiagnosticStage::StateMachine,
      FirmwareValidationDiagnosticReason::StateMachineRejected,
      static_cast<int32_t>(FirmwareValidationFailure::DowngradeRejected),
      FirmwareValidationFailure::DowngradeRejected};
  if (description.stage != FirmwareValidationDiagnosticStage::PartitionDescription ||
      description.reason != FirmwareValidationDiagnosticReason::PartitionDescriptionFailed ||
      identity.stage != FirmwareValidationDiagnosticStage::ImageIdentity ||
      identity.reason != FirmwareValidationDiagnosticReason::IdentityMismatch ||
      stateMachine.stage != FirmwareValidationDiagnosticStage::StateMachine ||
      stateMachine.validationFailure != FirmwareValidationFailure::DowngradeRejected) {
    return fail("stage-diagnostics");
  }

  DefaultFirmwareValidationProvider validator("plonk-and-play", "esp32-8048s070");
  FirmwareIdentity known{"plonk-and-play", "esp32-8048s070", "known", 10};
  FirmwareUpdateStateMachine machine(known, validator);
  if (!machine.beginRequest({true, true, false}) ||
      !machine.abortRequest(FirmwareValidationFailure::IntegrityFailure) ||
      machine.state() != FirmwareUpdateState::Failed ||
      machine.validationFailure() != FirmwareValidationFailure::IntegrityFailure ||
      machine.knownGood().version != 10 || machine.customerDataTouched()) {
    return fail("completed-invalid-not-incomplete");
  }

  FirmwareUpdateStatusView status;
  status.validationDiagnostic = identity;
  if (status.validationDiagnostic.stage != FirmwareValidationDiagnosticStage::ImageIdentity ||
      status.validationDiagnostic.reason != FirmwareValidationDiagnosticReason::IdentityMismatch ||
      status.validationDiagnostic.code != 0) {
    return fail("status-propagation");
  }

  std::cout << "WP6_VALIDATION_DIAGNOSTICS_PASS\n";
  return 0;
}
