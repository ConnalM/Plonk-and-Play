#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace pp {

// WP6 update state is controller-side state. Browser/Taster code may request an
// operation through the existing authority boundary, but never owns this state.
enum class FirmwareUpdateState : uint8_t {
  Idle,
  ReceivingStaging,
  Validating,
  CandidateReady,
  InstallingPendingBoot,
  AwaitingHealthConfirmation,
  Committed,
  RollingBack,
  KnownGoodRestored,
  RecoveryRequired,
  Failed,
};

enum class FirmwareUpdateRejection : uint8_t {
  None,
  PermissionDenied,
  LifecycleNotSettled,
  AlreadyPending,
  InvalidCandidateState,
  NotHealthConfirmation,
  RollbackNotInProgress,
};

enum class FirmwareValidationFailure : uint8_t {
  None,
  Unauthenticated,
  IncompatibleTarget,
  DowngradeRejected,
  Incomplete,
  IntegrityFailure,
};

struct FirmwareIdentity {
  const char* product = nullptr;
  const char* board = nullptr;
  const char* buildId = nullptr;
  uint32_t version = 0;
  constexpr FirmwareIdentity(const char* productValue = nullptr, const char* boardValue = nullptr,
                             const char* buildIdValue = nullptr, uint32_t versionValue = 0)
      : product(productValue), board(boardValue), buildId(buildIdValue), version(versionValue) {}
};

struct FirmwareCandidate {
  FirmwareIdentity identity{};
  size_t imageSize = 0;
  bool complete = false;
  bool integrityValid = false;
  bool authenticated = false;
  constexpr FirmwareCandidate(FirmwareIdentity identityValue = {}, size_t imageSizeValue = 0,
                              bool completeValue = false, bool integrityValidValue = false,
                              bool authenticatedValue = false)
      : identity(identityValue), imageSize(imageSizeValue), complete(completeValue),
        integrityValid(integrityValidValue), authenticated(authenticatedValue) {}
};

struct FirmwareUpdateRequestContext {
  bool controllingMaster = false;
  bool lifecycleSettled = false;
  // Only a separately controlled support/recovery path may set this true.
  bool supportDowngrade = false;
};

enum class FirmwareUpdateApiResult : uint8_t {
  Accepted,
  PermissionDenied,
  LifecycleNotSettled,
  AlreadyInProgress,
  InvalidRequest,
  TooLarge,
  IncompleteUpload,
  InvalidCandidate,
  NotReady,
  Rejected,
};

struct FirmwareUpdateStatusView {
  FirmwareUpdateState state = FirmwareUpdateState::Idle;
  FirmwareValidationFailure validationFailure = FirmwareValidationFailure::None;
  FirmwareUpdateRejection lastRejection = FirmwareUpdateRejection::None;
  size_t receivedBytes = 0;
  size_t expectedBytes = 0;
  uint32_t knownGoodVersion = 0;
  bool pendingReboot = false;
};

// The Browser talks to this controller-side seam. It deliberately exposes no
// storage, partition, or bootloader primitives; the platform adapter owns
// those operations behind the seam.
class FirmwareUpdateEndpoint {
 public:
  virtual ~FirmwareUpdateEndpoint() = default;
  virtual FirmwareUpdateApiResult begin(uint64_t owner, const FirmwareUpdateRequestContext& context,
                                        size_t expectedBytes) = 0;
  virtual FirmwareUpdateApiResult upload(uint64_t owner, const uint8_t* data, size_t size) = 0;
  virtual FirmwareUpdateApiResult validate(uint64_t owner) = 0;
  virtual FirmwareUpdateApiResult install(uint64_t owner) = 0;
  virtual void abort(uint64_t owner) = 0;
  virtual bool owns(uint64_t owner) const = 0;
  virtual FirmwareUpdateStatusView status() const = 0;
  virtual bool consumeRebootRequest() = 0;
};

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

  bool abortRequest() {
    if (state_ != FirmwareUpdateState::ReceivingStaging && state_ != FirmwareUpdateState::Validating) return false;
    state_ = FirmwareUpdateState::Failed;
    validationFailure_ = FirmwareValidationFailure::Incomplete;
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
