#pragma once

// The single physical SD/SPI owner for the ESP32-S3 product path. Logical
// History, active-detail and Diagnostic stores use this boundary and never
// initialise or mount the physical device themselves.

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "race_control.h"

namespace pp {

class PhysicalSdOwner final {
public:
  static constexpr int CsPin = 10;
  static constexpr int MosiPin = 11;
  static constexpr int SckPin = 12;
  static constexpr int MisoPin = 13;
  static constexpr uint32_t SpiFrequency = 20000000U;

  // Mounted means the physical device answered the SD mount call, but the logical
  // namespaces are still being reconstructed. Healthy is committed only by
  // commitRecovery() after that reconstruction succeeds.
  enum class Health : uint8_t { Unavailable, Mounted, Healthy, Fault };
  enum class RecoveryRequestResult : uint8_t {
    Accepted,
    AlreadyPending,
    AlreadyHealthy,
    LifecycleRejected,
  };

  class Lease final {
  public:
    explicit Lease(PhysicalSdOwner& owner) : owner_(&owner), held_(owner.take()) {}
    Lease(const Lease&) = delete;
    Lease& operator=(const Lease&) = delete;
    Lease(Lease&& other) noexcept : owner_(other.owner_), held_(other.held_) {
      other.owner_ = nullptr;
      other.held_ = false;
    }
    Lease& operator=(Lease&& other) noexcept {
      if (this != &other) {
        release();
        owner_ = other.owner_;
        held_ = other.held_;
        other.owner_ = nullptr;
        other.held_ = false;
      }
      return *this;
    }
    ~Lease() { release(); }
    explicit operator bool() const { return held_; }

    // A logical consumer may report a proven filesystem failure only while it
    // still holds this lease.  The owner remains the sole authority that
    // changes physical availability/health; this does not mount or recover.
    void reportIoFailure() {
      if (owner_ && held_) owner_->markIoFailureLocked();
    }

  private:
    void release() {
      if (owner_ && held_) owner_->give();
      owner_ = nullptr;
      held_ = false;
    }
    PhysicalSdOwner* owner_ = nullptr;
    bool held_ = false;
  };

  Lease lock() { return Lease(*this); }

  bool begin() {
    auto lease = lock();
    if (!lease) return false;
    if (mounted_) return true;
    return mountLocked();
  }

  // Physical recovery is deliberately owner-controlled and non-blocking. A
  // consumer cannot remount, and recovery is refused during a live session.
  bool recover() {
    auto lease = lock();
    if (!lease || !recoveryAllowed()) return false;
    if (mounted_) return true;
    return mountLocked();
  }

  // This is only an owner request.  It never probes or mounts the card; the
  // controller loop consumes the request and remains the sole recovery path.
  RecoveryRequestResult requestRecovery() {
    auto lease = lock();
    if (!lease || !recoveryAllowed()) return RecoveryRequestResult::LifecycleRejected;
    if (durable_) return RecoveryRequestResult::AlreadyHealthy;
    if (mounted_ || recoveryInProgress_) return RecoveryRequestResult::AlreadyPending;
    if (recoveryPending_) return RecoveryRequestResult::AlreadyPending;
    recoveryPending_ = true;
    return RecoveryRequestResult::Accepted;
  }

  bool recoveryRequestPending() const { return recoveryPending_; }

  // Consumption is deliberately one-shot.  The caller must be the existing
  // lifecycle-gated owner service, never a storage consumer.
  bool consumeRecoveryRequest() {
    auto lease = lock();
    if (!lease || !recoveryPending_) return false;
    recoveryPending_ = false;
    return true;
  }

  void markRecoveryFailure() {
    auto lease = lock();
    if (lease) markIoFailureLocked();
  }

  // The controller recovery service calls this only after every required
  // namespace has reconstructed successfully.
  void commitRecovery() {
    auto lease = lock();
    if (lease && mounted_) {
      recoveryInProgress_ = false;
      durable_ = true;
      health_ = Health::Healthy;
    }
  }

  void setLifecycle(SessionLifecycle lifecycle) { lifecycle_ = lifecycle; }
  SessionLifecycle lifecycle() const { return lifecycle_; }
  bool recoveryAllowed() const {
    switch (lifecycle_) {
      case SessionLifecycle::Ready:
      case SessionLifecycle::Finished:
        return true;
      case SessionLifecycle::Starting:
      case SessionLifecycle::Racing:
      case SessionLifecycle::Paused:
      case SessionLifecycle::Restarting:
      case SessionLifecycle::Faulted:
        return false;
    }
    return false;
  }

  // Physical availability is intentionally distinct from durable health.
  bool available() const { return mounted_; }
  bool durable() const { return durable_; }
  Health health() const { return health_; }

  // Callers must hold Lease while using these wrappers.  The non-blocking
  // lease is the bounded arbitration boundary; it is intentionally not
  // hidden inside individual File operations.
  File open(const char* path, const char* mode) { return SD.open(path, mode); }
  File open(const String& path, const char* mode) { return SD.open(path, mode); }
  bool exists(const char* path) const { return SD.exists(path); }
  bool mkdir(const char* path) { return SD.mkdir(path); }
  bool mkdir(const String& path) { return SD.mkdir(path); }
  bool remove(const char* path) { return SD.remove(path); }
  bool remove(const String& path) { return SD.remove(path); }
  bool rename(const char* from, const char* to) { return SD.rename(from, to); }
  bool rename(const String& from, const String& to) { return SD.rename(from, to); }
  uint64_t cardSize() const { return static_cast<uint64_t>(SD.cardSize()); }
  uint64_t totalBytes() const { return static_cast<uint64_t>(SD.totalBytes()); }
  uint64_t usedBytes() const { return static_cast<uint64_t>(SD.usedBytes()); }

private:
  bool take() {
    if (!mutex_) mutex_ = xSemaphoreCreateRecursiveMutex();
    return mutex_ && xSemaphoreTakeRecursive(mutex_, 0) == pdTRUE;
  }
  void give() {
    if (mutex_) xSemaphoreGiveRecursive(mutex_);
  }
  void markIoFailureLocked() {
    mounted_ = false;
    recoveryInProgress_ = false;
    durable_ = false;
    health_ = Health::Unavailable;
  }
  bool mountLocked() {
    mounted_ = false;
    Serial.printf("[P8-P1] SD owner mount begin cs=%d mosi=%d sck=%d miso=%d\n",
                  CsPin, MosiPin, SckPin, MisoPin);
    spi_.begin(SckPin, MisoPin, MosiPin, CsPin);
    mounted_ = SD.begin(CsPin, spi_, SpiFrequency);
    recoveryInProgress_ = mounted_;
    durable_ = false;
    health_ = mounted_ ? Health::Mounted : Health::Unavailable;
    Serial.printf("[P8-P1] SD owner mount %s\n", mounted_ ? "OK" : "FAILED");
    return mounted_;
  }

  SPIClass spi_{FSPI};
  SemaphoreHandle_t mutex_ = nullptr;
  volatile bool mounted_ = false;
  volatile bool recoveryInProgress_ = false;
  volatile bool durable_ = false;
  volatile Health health_ = Health::Unavailable;
  volatile bool recoveryPending_ = false;
  SessionLifecycle lifecycle_ = SessionLifecycle::Ready;
};

} // namespace pp
