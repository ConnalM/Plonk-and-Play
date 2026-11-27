#pragma once

#include <string.h>

#include "pp/firmware_update_api.h"
#include "pp/signed_update_envelope.h"

namespace pp {

inline FirmwareValidationDiagnosticReason firmwareValidationDiagnosticReason(SignedEnvelopeResult result) {
  switch (result) {
    case SignedEnvelopeResult::UnsupportedVersion: return FirmwareValidationDiagnosticReason::UnsupportedVersion;
    case SignedEnvelopeResult::Malformed: return FirmwareValidationDiagnosticReason::Malformed;
    case SignedEnvelopeResult::Truncated: return FirmwareValidationDiagnosticReason::Truncated;
    case SignedEnvelopeResult::SizeMismatch: return FirmwareValidationDiagnosticReason::SizeMismatch;
    case SignedEnvelopeResult::ProductMismatch: return FirmwareValidationDiagnosticReason::ProductMismatch;
    case SignedEnvelopeResult::BoardMismatch: return FirmwareValidationDiagnosticReason::BoardMismatch;
    case SignedEnvelopeResult::DigestMismatch: return FirmwareValidationDiagnosticReason::DigestMismatch;
    case SignedEnvelopeResult::Unsigned: return FirmwareValidationDiagnosticReason::Unsigned;
    case SignedEnvelopeResult::InvalidSignature: return FirmwareValidationDiagnosticReason::InvalidSignature;
    case SignedEnvelopeResult::VerificationUnavailable: return FirmwareValidationDiagnosticReason::VerificationUnavailable;
    case SignedEnvelopeResult::Ok: return FirmwareValidationDiagnosticReason::None;
  }
  return FirmwareValidationDiagnosticReason::Malformed;
}

inline FirmwareValidationFailure firmwareValidationFailure(SignedEnvelopeResult result) {
  switch (result) {
    case SignedEnvelopeResult::ProductMismatch:
    case SignedEnvelopeResult::BoardMismatch:
      return FirmwareValidationFailure::IncompatibleTarget;
    case SignedEnvelopeResult::Unsigned:
    case SignedEnvelopeResult::InvalidSignature:
      return FirmwareValidationFailure::Unauthenticated;
    case SignedEnvelopeResult::Ok:
      return FirmwareValidationFailure::None;
    default:
      return FirmwareValidationFailure::IntegrityFailure;
  }
}

struct FirmwareValidationResult {
  bool accepted = false;
  FirmwareValidationFailure failure = FirmwareValidationFailure::None;
};

// A measurable validation seam. Production code can bind this to the signed
// image/compatibility implementation; host tests use the deterministic default.
class FirmwareValidationProvider {
 public:
  virtual ~FirmwareValidationProvider() = default;
  virtual FirmwareValidationResult validate(const FirmwareCandidate& candidate,
                                            const FirmwareIdentity& knownGood,
                                            bool supportDowngrade) const = 0;
};

class DefaultFirmwareValidationProvider final : public FirmwareValidationProvider {
 public:
  DefaultFirmwareValidationProvider(const char* product, const char* board)
      : product_(product), board_(board) {}

  FirmwareValidationResult validate(const FirmwareCandidate& candidate,
                                    const FirmwareIdentity& knownGood,
                                    bool supportDowngrade) const override {
    if (!candidate.authenticated) return result(false, FirmwareValidationFailure::Unauthenticated);
    if (!candidate.complete) return result(false, FirmwareValidationFailure::Incomplete);
    if (!candidate.integrityValid) return result(false, FirmwareValidationFailure::IntegrityFailure);
    if (!same(candidate.identity.product, product_) || !same(candidate.identity.board, board_)) {
      return result(false, FirmwareValidationFailure::IncompatibleTarget);
    }
    if (!supportDowngrade && candidate.identity.version < knownGood.version) {
      return result(false, FirmwareValidationFailure::DowngradeRejected);
    }
    return result(true, FirmwareValidationFailure::None);
  }

 private:
  static FirmwareValidationResult result(bool accepted, FirmwareValidationFailure failure) {
    FirmwareValidationResult value;
    value.accepted = accepted;
    value.failure = failure;
    return value;
  }

  static bool same(const char* a, const char* b) {
    return a && b && strcmp(a, b) == 0;
  }
  const char* product_;
  const char* board_;
};

class FirmwareUpdateStateMachine {
 public:
  FirmwareUpdateStateMachine(const FirmwareIdentity& knownGood,
                             const FirmwareValidationProvider& validator)
      : knownGood_(knownGood), validator_(validator) {}

  FirmwareUpdateState state() const { return state_; }
  const FirmwareIdentity& knownGood() const { return knownGood_; }
  const FirmwareCandidate& candidate() const { return candidate_; }
  FirmwareValidationFailure validationFailure() const { return validationFailure_; }
  FirmwareUpdateRejection lastRejection() const { return lastRejection_; }
  bool customerDataTouched() const { return false; }

  // Platform adapters call these only while reconstructing the boot boundary;
  // they do not perform any flash or filesystem work themselves.
  bool setKnownGoodIdentity(const FirmwareIdentity& identity) {
    if (state_ != FirmwareUpdateState::Idle) return false;
    knownGood_ = identity;
    return true;
  }

  bool restorePendingCandidate(const FirmwareCandidate& candidate) {
    if (state_ != FirmwareUpdateState::Idle) return false;
    candidate_ = candidate;
    validationFailure_ = FirmwareValidationFailure::None;
    state_ = FirmwareUpdateState::AwaitingHealthConfirmation;
    return true;
  }

  bool beginRequest(const FirmwareUpdateRequestContext& context) {
    lastRejection_ = FirmwareUpdateRejection::None;
    if (!context.controllingMaster) return reject(FirmwareUpdateRejection::PermissionDenied);
    if (!context.lifecycleSettled) return reject(FirmwareUpdateRejection::LifecycleNotSettled);
    if (pending()) return reject(FirmwareUpdateRejection::AlreadyPending);
    state_ = FirmwareUpdateState::ReceivingStaging;
    candidate_ = {};
    validationFailure_ = FirmwareValidationFailure::None;
    supportDowngrade_ = context.supportDowngrade;
    return true;
  }

  bool abortRequest(FirmwareValidationFailure failure = FirmwareValidationFailure::Incomplete) {
    if (state_ != FirmwareUpdateState::ReceivingStaging && state_ != FirmwareUpdateState::Validating) return false;
    state_ = FirmwareUpdateState::Failed;
    validationFailure_ = failure;
    return true;
  }

  // The caller supplies the completed staged image descriptor. No filesystem,
  // flash or network operation is performed by this state machine.
  bool finishStaging(const FirmwareCandidate& candidate) {
    if (state_ != FirmwareUpdateState::ReceivingStaging) {
      lastRejection_ = FirmwareUpdateRejection::InvalidCandidateState;
      return false;
    }
    candidate_ = candidate;
    state_ = FirmwareUpdateState::Validating;
    const FirmwareValidationResult result = validator_.validate(candidate_, knownGood_, supportDowngrade_);
    if (!result.accepted) {
      validationFailure_ = result.failure;
      state_ = FirmwareUpdateState::Failed;
      return false;
    }
    state_ = FirmwareUpdateState::CandidateReady;
    return true;
  }

  // Installation is represented as a boundary only. The platform updater owns
  // the actual slot write and reboot; it must call healthConfirmation() later.
  bool beginInstall() {
    if (state_ != FirmwareUpdateState::CandidateReady) {
      lastRejection_ = FirmwareUpdateRejection::InvalidCandidateState;
      return false;
    }
    state_ = FirmwareUpdateState::InstallingPendingBoot;
    state_ = FirmwareUpdateState::AwaitingHealthConfirmation;
    return true;
  }

  bool healthConfirmation(bool healthy) {
    if (state_ != FirmwareUpdateState::AwaitingHealthConfirmation) {
      lastRejection_ = FirmwareUpdateRejection::NotHealthConfirmation;
      return false;
    }
    if (healthy) {
      knownGood_ = candidate_.identity;
      state_ = FirmwareUpdateState::Committed;
      return true;
    }
    state_ = FirmwareUpdateState::RollingBack;
    return false;
  }

  // The platform boot/update adapter calls this after attempting rollback. A
  // successful rollback restores ordinary operation; support mode is reserved
  // for the case where rollback itself cannot restore the known-good image.
  bool completeRollback(bool rollbackSucceeded) {
    if (state_ != FirmwareUpdateState::RollingBack) {
      lastRejection_ = FirmwareUpdateRejection::RollbackNotInProgress;
      return false;
    }
    state_ = rollbackSucceeded ? FirmwareUpdateState::KnownGoodRestored : FirmwareUpdateState::RecoveryRequired;
    return rollbackSucceeded;
  }

 private:
  bool pending() const {
    return state_ != FirmwareUpdateState::Idle && state_ != FirmwareUpdateState::Failed &&
           state_ != FirmwareUpdateState::RecoveryRequired && state_ != FirmwareUpdateState::Committed &&
           state_ != FirmwareUpdateState::KnownGoodRestored;
  }
  bool reject(FirmwareUpdateRejection reason) {
    lastRejection_ = reason;
    return false;
  }

  FirmwareUpdateState state_ = FirmwareUpdateState::Idle;
  FirmwareIdentity knownGood_{};
  FirmwareCandidate candidate_{};
  const FirmwareValidationProvider& validator_;
  FirmwareValidationFailure validationFailure_ = FirmwareValidationFailure::None;
  FirmwareUpdateRejection lastRejection_ = FirmwareUpdateRejection::None;
  bool supportDowngrade_ = false;
};

}  // namespace pp
