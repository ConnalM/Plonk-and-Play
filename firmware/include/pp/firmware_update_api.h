#pragma once

#include <stddef.h>
#include <stdint.h>

namespace pp {

// Controller-facing update and maintenance API.  Browser code uses this
// key-independent boundary; signed package verification remains in
// firmware_update.h and its verifier-specific dependency.
enum class FirmwareUpdateState : uint8_t {
  Idle, ReceivingStaging, Validating, CandidateReady, InstallingPendingBoot,
  AwaitingHealthConfirmation, Committed, RollingBack, KnownGoodRestored,
  RecoveryRequired, Failed,
};

enum class FirmwareUpdateRejection : uint8_t {
  None, PermissionDenied, LifecycleNotSettled, AlreadyPending,
  InvalidCandidateState, NotHealthConfirmation, RollbackNotInProgress,
};

enum class FirmwareValidationFailure : uint8_t {
  None, Unauthenticated, IncompatibleTarget, DowngradeRejected, Incomplete,
  IntegrityFailure,
};

enum class FirmwareValidationDiagnosticStage : uint8_t {
  None, Ppu1Verifier, EspOtaEnd, PartitionDescription, ImageIdentity,
  StateMachine,
};

enum class FirmwareValidationDiagnosticReason : uint8_t {
  None, UnsupportedVersion, Malformed, Truncated, SizeMismatch, ProductMismatch,
  BoardMismatch, DigestMismatch, Unsigned, InvalidSignature,
  VerificationUnavailable, OtaEndFailed, PartitionDescriptionFailed,
  IdentityMismatch, StateMachineRejected,
};

struct FirmwareValidationDiagnostic {
  FirmwareValidationDiagnosticStage stage = FirmwareValidationDiagnosticStage::None;
  FirmwareValidationDiagnosticReason reason = FirmwareValidationDiagnosticReason::None;
  int32_t code = 0;
  FirmwareValidationFailure validationFailure = FirmwareValidationFailure::None;
};

inline bool recordFirstFirmwareValidationDiagnostic(FirmwareValidationDiagnostic& current,
                                                     const FirmwareValidationDiagnostic& next) {
  if (current.stage != FirmwareValidationDiagnosticStage::None) return false;
  current = next;
  return true;
}

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
  bool supportDowngrade = false;
};

enum class FirmwareUpdateApiResult : uint8_t {
  Accepted, PermissionDenied, LifecycleNotSettled, AlreadyInProgress,
  InvalidRequest, TooLarge, IncompleteUpload, InvalidCandidate, NotReady,
  NoUpdateAvailable, OnlineProviderUnavailable, OnlineDownloadFailed, Rejected,
};

struct FirmwareOnlineOffer {
  FirmwareIdentity identity{};
  size_t imageSize = 0;
  bool available = false;
};

class OnlineFirmwareProvider {
 public:
  virtual ~OnlineFirmwareProvider() = default;
  virtual FirmwareUpdateApiResult check(const FirmwareIdentity& current,
                                        FirmwareOnlineOffer& offer) = 0;
  virtual FirmwareUpdateApiResult beginDownload(const FirmwareOnlineOffer& offer) = 0;
  virtual size_t read(uint8_t* buffer, size_t capacity) = 0;
};

struct FirmwareUpdateStatusView {
  FirmwareUpdateState state = FirmwareUpdateState::Idle;
  FirmwareValidationFailure validationFailure = FirmwareValidationFailure::None;
  FirmwareValidationDiagnostic validationDiagnostic{};
  FirmwareUpdateRejection lastRejection = FirmwareUpdateRejection::None;
  size_t receivedBytes = 0;
  size_t expectedBytes = 0;
  uint32_t knownGoodVersion = 0;
  bool pendingReboot = false;
  FirmwareOnlineOffer onlineOffer{};
};

class FirmwareUpdateEndpoint {
 public:
  virtual ~FirmwareUpdateEndpoint() = default;
  virtual FirmwareUpdateApiResult begin(uint64_t owner, const FirmwareUpdateRequestContext& context,
                                        size_t expectedBytes) = 0;
  virtual FirmwareUpdateApiResult upload(uint64_t owner, const uint8_t* data, size_t size) = 0;
  virtual FirmwareUpdateApiResult validate(uint64_t owner) = 0;
  virtual FirmwareUpdateApiResult install(uint64_t owner,
                                          const FirmwareUpdateRequestContext& context) = 0;
  virtual void abort(uint64_t owner) = 0;
  virtual bool owns(uint64_t owner) const = 0;
  virtual FirmwareUpdateStatusView status() const = 0;
  virtual bool consumeRebootRequest() = 0;
  virtual FirmwareUpdateApiResult checkOnline(uint64_t owner,
                                               const FirmwareUpdateRequestContext& context,
                                               FirmwareOnlineOffer& offer) {
    (void)owner; (void)context; offer = {}; return FirmwareUpdateApiResult::OnlineProviderUnavailable;
  }
  virtual FirmwareUpdateApiResult startOnline(uint64_t owner,
                                               const FirmwareUpdateRequestContext& context) {
    (void)owner; (void)context; return FirmwareUpdateApiResult::OnlineProviderUnavailable;
  }
};

enum class MaintenanceApiResult : uint8_t {
  Accepted, PermissionDenied, LifecycleNotSettled, ConfirmationRequired, Busy, Failed,
};

struct MaintenanceRequestContext {
  bool controllingMaster = false;
  bool lifecycleSettled = false;
  bool confirmed = false;
  bool strongConfirmation = false;
};

class MaintenanceEndpoint {
 public:
  virtual ~MaintenanceEndpoint() = default;
  virtual MaintenanceApiResult restart(uint64_t owner, const MaintenanceRequestContext& context) = 0;
  virtual MaintenanceApiResult resetSettings(uint64_t owner, const MaintenanceRequestContext& context) = 0;
  virtual MaintenanceApiResult eraseEverything(uint64_t owner, const MaintenanceRequestContext& context) = 0;
  virtual bool consumeRebootRequest() = 0;
};

}  // namespace pp
