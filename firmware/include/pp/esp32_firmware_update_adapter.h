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
}  // namespace pp

#endif
