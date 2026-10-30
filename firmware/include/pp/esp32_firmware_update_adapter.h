#pragma once

// Thin ESP32 platform adapter for the WP6 controller-side update state
// machine.  It owns only OTA partition/bootloader operations; Browser/API
// code remains outside this boundary.

#include "pp/firmware_update.h"

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)

#include <Arduino.h>
#include <esp_image_format.h>
#include <esp_err.h>
#include <esp_ota_ops.h>
#if __has_include(<esp_idf_version.h>)
#include <esp_idf_version.h>
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pp {

enum class FirmwarePlatformResult : uint8_t {
  Ok,
  NotReady,
  InvalidState,
  Capacity,
  WriteFailed,
  ValidationFailed,
  RollbackFailed,
};

class Esp32FirmwareUpdateAdapter final {
 public:
  explicit Esp32FirmwareUpdateAdapter(FirmwareUpdateStateMachine* stateMachine = nullptr)
      : stateMachine_(stateMachine) {}

  // Read-only initialization.  This does not erase, mount, select, or reboot
  // an image.  It records the currently running slot as the known-good slot.
  bool initialize() {
    running_ = esp_ota_get_running_partition();
    next_ = esp_ota_get_next_update_partition(running_);
    // Fresh Wokwi images do not contain an initialized otadata record yet.
    // Keep production on the native round-robin selection, but discover the
    // inactive OTA slot directly so adapter startup remains observable there.
    if (!next_) {
      const esp_partition_t* ota0 = esp_partition_find_first(
          ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
      const esp_partition_t* ota1 = esp_partition_find_first(
          ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
      next_ = running_ == ota0 ? ota1 : ota0;
    }
    if (!running_ || !next_) {
      Serial.println("[WP6 OTA] adapter initialization FAILED no OTA slots");
      initialized_ = false;
      return false;
    }
    copyDescription(running_, knownGoodDescription_);
    initialized_ = true;
    esp_ota_img_states_t state{};
    const esp_err_t stateResult = esp_ota_get_state_partition(running_, &state);
    pendingBoot_ = stateResult == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY;
    if (stateMachine_) {
      FirmwareIdentity knownIdentity = identityFromDescription(knownGoodDescription_);
      if (pendingBoot_) {
        // On a pending candidate boot, the inactive OTA slot is the previous
        // known-good image. Reconstruct both identities before health gating.
        esp_app_desc_t previous{};
        if (esp_ota_get_partition_description(next_, &previous) == ESP_OK) {
          knownGoodDescription_ = previous;
          knownIdentity = identityFromDescription(knownGoodDescription_);
          candidateDescription_ = {};
          copyDescription(running_, candidateDescription_);
          FirmwareCandidate pendingCandidate = candidateFromDescription(candidateDescription_, 0);
          stateMachine_->setKnownGoodIdentity(knownIdentity);
          stateMachine_->restorePendingCandidate(pendingCandidate);
        }
      } else {
        stateMachine_->setKnownGoodIdentity(knownIdentity);
      }
    }
    Serial.printf("[WP6 OTA] adapter initialized running=%s next=%s pending=%u rollback=%u\n",
                  running_->label, next_->label, pendingBoot_ ? 1u : 0u,
#if defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE)
                  1u
#else
                  0u
#endif
    );
    return true;
  }

  bool initialized() const { return initialized_; }
  bool pendingBoot() const { return pendingBoot_; }
  const esp_partition_t* runningPartition() const { return running_; }
  const esp_partition_t* candidatePartition() const { return next_; }
  const esp_app_desc_t& knownGoodDescription() const { return knownGoodDescription_; }
  size_t bytesWritten() const { return bytesWritten_; }
  size_t candidateCapacity() const { return next_ ? next_->size : 0; }
  bool stagingActive() const { return handleActive_; }

  FirmwarePlatformResult abortStaging() {
    if (!handleActive_) return FirmwarePlatformResult::InvalidState;
    const esp_err_t result = esp_ota_abort(handle_);
    handleActive_ = false;
    handle_ = 0;
    return result == ESP_OK ? FirmwarePlatformResult::Ok : FirmwarePlatformResult::WriteFailed;
  }

  // Stage a complete candidate into the inactive OTA slot.  esp_ota_end is
  // the framework integrity/signature validation boundary; product identity,
  // board compatibility and version policy remain the state-machine validator.
  FirmwarePlatformResult beginStaging(size_t imageSize = OTA_SIZE_UNKNOWN) {
    if (!initialized_ || !next_ || handleActive_) return FirmwarePlatformResult::InvalidState;
    if (imageSize != OTA_SIZE_UNKNOWN && imageSize > next_->size) return FirmwarePlatformResult::Capacity;
    if (esp_ota_begin(next_, imageSize, &handle_) != ESP_OK) return FirmwarePlatformResult::WriteFailed;
    handleActive_ = true;
    bytesWritten_ = 0;
    return FirmwarePlatformResult::Ok;
  }

  FirmwarePlatformResult writeStaging(const uint8_t* data, size_t size) {
    if (!handleActive_ || !data || !size) return FirmwarePlatformResult::InvalidState;
    if (bytesWritten_ + size > next_->size) return FirmwarePlatformResult::Capacity;
    if (esp_ota_write(handle_, data, size) != ESP_OK) return FirmwarePlatformResult::WriteFailed;
    bytesWritten_ += size;
    return FirmwarePlatformResult::Ok;
  }

  FirmwarePlatformResult finishStaging(FirmwareCandidate& candidate) {
    if (!handleActive_) return FirmwarePlatformResult::InvalidState;
    const esp_err_t result = esp_ota_end(handle_);
    handleActive_ = false;
    handle_ = 0;
    if (result != ESP_OK) return FirmwarePlatformResult::ValidationFailed;

    esp_app_desc_t description{};
    if (esp_ota_get_partition_description(next_, &description) != ESP_OK) {
      return FirmwarePlatformResult::ValidationFailed;
    }
    candidateDescription_ = description;
    candidate = {};
    candidate.identity = identityFromDescription(candidateDescription_);
    candidate.imageSize = bytesWritten_;
    candidate.complete = true;
    candidate.integrityValid = true;
    // esp_ota_end performs the platform's image validation (and signature
    // validation when secure boot/signing is enabled).  The higher-level
    // validator remains the authority for product identity and policy.
    candidate.authenticated = true;
    if (stateMachine_ && !stateMachine_->finishStaging(candidate)) {
      return FirmwarePlatformResult::ValidationFailed;
    }
    return FirmwarePlatformResult::Ok;
  }

  // Select the validated inactive slot and move the state machine to its
  // post-reboot health-confirmation boundary.  This does not reboot.
  FirmwarePlatformResult selectCandidate() {
    if (!initialized_ || !next_ || !candidateDescription_.magic_word) return FirmwarePlatformResult::InvalidState;
    if (esp_ota_set_boot_partition(next_) != ESP_OK) return FirmwarePlatformResult::WriteFailed;
    if (stateMachine_ && !stateMachine_->beginInstall()) {
      // Never leave a boot-selection side effect behind a rejected state
      // transition; restore the previously running known-good slot.
      esp_ota_set_boot_partition(running_);
      return FirmwarePlatformResult::InvalidState;
    }
    pendingBoot_ = true;
    return FirmwarePlatformResult::Ok;
  }

  // Called after normal startup health checks on the candidate boot.
  FirmwarePlatformResult confirmHealth(bool healthy) {
    if (!initialized_ || !pendingBoot_) return FirmwarePlatformResult::InvalidState;
    if (healthy) {
      if (stateMachine_ && !stateMachine_->healthConfirmation(true)) return FirmwarePlatformResult::InvalidState;
      if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK) return FirmwarePlatformResult::ValidationFailed;
      pendingBoot_ = false;
      return FirmwarePlatformResult::Ok;
    }

    if (stateMachine_) stateMachine_->healthConfirmation(false);
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
    const esp_err_t rollback = esp_ota_mark_app_invalid_rollback();
#else
    // Older Arduino/IDF combinations expose only the rebooting rollback API.
    // It is still the bootloader-owned rollback boundary; successful return is
    // not expected because the call restarts immediately.
    const esp_err_t rollback = esp_ota_mark_app_invalid_rollback_and_reboot();
#endif
    const bool restored = rollback == ESP_OK;
    if (stateMachine_) stateMachine_->completeRollback(restored);
    if (!restored) return FirmwarePlatformResult::RollbackFailed;
    pendingBoot_ = false;
    esp_restart();
    return FirmwarePlatformResult::Ok;  // unreachable on real hardware
  }

  static const char* boardName() { return "esp32-8048s070"; }

 private:
  static FirmwareIdentity identityFromDescription(const esp_app_desc_t& description) {
    FirmwareIdentity identity;
    identity.product = description.project_name;
    identity.board = boardName();
    identity.buildId = description.version;
    identity.version = description.secure_version;
    return identity;
  }

  static FirmwareCandidate candidateFromDescription(const esp_app_desc_t& description, size_t imageSize) {
    FirmwareCandidate candidate;
    candidate.identity = identityFromDescription(description);
    candidate.imageSize = imageSize;
    candidate.complete = true;
    candidate.integrityValid = true;
    candidate.authenticated = true;
    return candidate;
  }

  static void copyDescription(const esp_partition_t* partition, esp_app_desc_t& out) {
    if (!partition || esp_ota_get_partition_description(partition, &out) != ESP_OK) out = {};
  }

  FirmwareUpdateStateMachine* stateMachine_ = nullptr;
  const esp_partition_t* running_ = nullptr;
  const esp_partition_t* next_ = nullptr;
  esp_ota_handle_t handle_ = 0;
  size_t bytesWritten_ = 0;
  bool initialized_ = false;
  bool handleActive_ = false;
  bool pendingBoot_ = false;
  esp_app_desc_t knownGoodDescription_{};
  esp_app_desc_t candidateDescription_{};
};

// Controller-owned Browser/update boundary. It coordinates the already
// approved state machine with the ESP32 adapter but exposes no OTA primitives
// to Browser code.
class Esp32FirmwareUpdateService final : public FirmwareUpdateEndpoint {
 public:
  Esp32FirmwareUpdateService(FirmwareUpdateStateMachine& stateMachine,
                             Esp32FirmwareUpdateAdapter& adapter)
      : stateMachine_(stateMachine), adapter_(adapter) {}

  FirmwareUpdateApiResult begin(uint64_t owner, const FirmwareUpdateRequestContext& context,
                                size_t expectedBytes) override {
    if (!owner || !context.controllingMaster) return FirmwareUpdateApiResult::PermissionDenied;
    if (!context.lifecycleSettled) return FirmwareUpdateApiResult::LifecycleNotSettled;
    if (expectedBytes == 0) return FirmwareUpdateApiResult::InvalidRequest;
    if (stateMachine_.state() != FirmwareUpdateState::Idle &&
        stateMachine_.state() != FirmwareUpdateState::Failed &&
        stateMachine_.state() != FirmwareUpdateState::Committed &&
        stateMachine_.state() != FirmwareUpdateState::KnownGoodRestored &&
        stateMachine_.state() != FirmwareUpdateState::RecoveryRequired) {
      return FirmwareUpdateApiResult::AlreadyInProgress;
    }
    if (expectedBytes > adapter_.candidateCapacity()) return FirmwareUpdateApiResult::TooLarge;
    if (!stateMachine_.beginRequest(context)) return FirmwareUpdateApiResult::Rejected;
    const FirmwarePlatformResult result = adapter_.beginStaging(expectedBytes);
    if (result != FirmwarePlatformResult::Ok) {
      stateMachine_.abortRequest();
      return result == FirmwarePlatformResult::Capacity ? FirmwareUpdateApiResult::TooLarge : FirmwareUpdateApiResult::Rejected;
    }
    owner_ = owner;
    expectedBytes_ = expectedBytes;
    receivedBytes_ = 0;
    return FirmwareUpdateApiResult::Accepted;
  }

  FirmwareUpdateApiResult upload(uint64_t owner, const uint8_t* data, size_t size) override {
    if (!ownerMatches(owner) || stateMachine_.state() != FirmwareUpdateState::ReceivingStaging) return FirmwareUpdateApiResult::NotReady;
    if (!data || size == 0 || receivedBytes_ + size > expectedBytes_) return FirmwareUpdateApiResult::InvalidRequest;
    const FirmwarePlatformResult result = adapter_.writeStaging(data, size);
    if (result != FirmwarePlatformResult::Ok) {
      adapter_.abortStaging();
      stateMachine_.abortRequest();
      return result == FirmwarePlatformResult::Capacity ? FirmwareUpdateApiResult::TooLarge : FirmwareUpdateApiResult::Rejected;
    }
    receivedBytes_ += size;
    return FirmwareUpdateApiResult::Accepted;
  }

  FirmwareUpdateApiResult validate(uint64_t owner) override {
    if (!ownerMatches(owner) || stateMachine_.state() != FirmwareUpdateState::ReceivingStaging) return FirmwareUpdateApiResult::NotReady;
    if (receivedBytes_ != expectedBytes_) {
      adapter_.abortStaging();
      stateMachine_.abortRequest();
      return FirmwareUpdateApiResult::IncompleteUpload;
    }
    FirmwareCandidate candidate{};
    const FirmwarePlatformResult result = adapter_.finishStaging(candidate);
    if (result != FirmwarePlatformResult::Ok) {
      stateMachine_.abortRequest();
      return FirmwareUpdateApiResult::InvalidCandidate;
    }
    return FirmwareUpdateApiResult::Accepted;
  }

  FirmwareUpdateApiResult install(uint64_t owner) override {
    if (!ownerMatches(owner)) return FirmwareUpdateApiResult::PermissionDenied;
    if (stateMachine_.state() != FirmwareUpdateState::CandidateReady) return FirmwareUpdateApiResult::InvalidCandidate;
    if (adapter_.selectCandidate() != FirmwarePlatformResult::Ok) return FirmwareUpdateApiResult::Rejected;
    rebootRequested_ = true;
    return FirmwareUpdateApiResult::Accepted;
  }

  void abort(uint64_t owner) override {
    if (!ownerMatches(owner)) return;
    if (adapter_.stagingActive()) adapter_.abortStaging();
    stateMachine_.abortRequest();
    owner_ = 0;
    expectedBytes_ = receivedBytes_ = 0;
  }

  bool owns(uint64_t owner) const override { return ownerMatches(owner); }

  FirmwareUpdateStatusView status() const override {
    FirmwareUpdateStatusView view;
    view.state = stateMachine_.state();
    view.validationFailure = stateMachine_.validationFailure();
    view.lastRejection = stateMachine_.lastRejection();
    view.receivedBytes = receivedBytes_;
    view.expectedBytes = expectedBytes_;
    view.knownGoodVersion = stateMachine_.knownGood().version;
    view.pendingReboot = rebootRequested_;
    return view;
  }

  bool consumeRebootRequest() override {
    const bool pending = rebootRequested_;
    rebootRequested_ = false;
    return pending;
  }

 private:
  bool ownerMatches(uint64_t owner) const { return owner != 0 && owner_ == owner; }
  FirmwareUpdateStateMachine& stateMachine_;
  Esp32FirmwareUpdateAdapter& adapter_;
  uint64_t owner_ = 0;
  size_t expectedBytes_ = 0;
  size_t receivedBytes_ = 0;
  bool rebootRequested_ = false;
};

}  // namespace pp

#else

namespace pp {
// Host builds use deterministic fakes; the ESP32 implementation above is
// intentionally unavailable outside the platform SDK.
class Esp32FirmwareUpdateAdapter final {
 public:
  bool initialize() { return false; }
  bool initialized() const { return false; }
};
class Esp32FirmwareUpdateService final : public FirmwareUpdateEndpoint {
 public:
  Esp32FirmwareUpdateService(FirmwareUpdateStateMachine&, Esp32FirmwareUpdateAdapter&) {}
  FirmwareUpdateApiResult begin(uint64_t, const FirmwareUpdateRequestContext&, size_t) override { return FirmwareUpdateApiResult::Rejected; }
  FirmwareUpdateApiResult upload(uint64_t, const uint8_t*, size_t) override { return FirmwareUpdateApiResult::Rejected; }
  FirmwareUpdateApiResult validate(uint64_t) override { return FirmwareUpdateApiResult::Rejected; }
  FirmwareUpdateApiResult install(uint64_t) override { return FirmwareUpdateApiResult::Rejected; }
  void abort(uint64_t) override {}
  bool owns(uint64_t) const override { return false; }
  FirmwareUpdateStatusView status() const override { return {}; }
  bool consumeRebootRequest() override { return false; }
};
}  // namespace pp

#endif
