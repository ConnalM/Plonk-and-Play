#include "pp/firmware_update.h"

#include <cstring>
#include <iostream>

using namespace pp;

class FakeUpdateEndpoint final : public FirmwareUpdateEndpoint {
 public:
  FakeUpdateEndpoint() : validator_("plonk-and-play", "esp32-8048s070"), machine_(known_, validator_) {}

  FirmwareUpdateApiResult begin(uint64_t owner, const FirmwareUpdateRequestContext& context,
                                size_t expectedBytes) override {
    if (!owner || !context.controllingMaster) return FirmwareUpdateApiResult::PermissionDenied;
    if (!context.lifecycleSettled) return FirmwareUpdateApiResult::LifecycleNotSettled;
    if (!expectedBytes) return FirmwareUpdateApiResult::InvalidRequest;
    if (machine_.state() != FirmwareUpdateState::Idle && machine_.state() != FirmwareUpdateState::Failed &&
        machine_.state() != FirmwareUpdateState::Committed && machine_.state() != FirmwareUpdateState::KnownGoodRestored)
      return FirmwareUpdateApiResult::AlreadyInProgress;
    if (expectedBytes > Capacity) return FirmwareUpdateApiResult::TooLarge;
    if (!machine_.beginRequest(context)) return FirmwareUpdateApiResult::Rejected;
    owner_ = owner; expected_ = expectedBytes; received_ = 0; return FirmwareUpdateApiResult::Accepted;
  }
  FirmwareUpdateApiResult upload(uint64_t owner, const uint8_t* data, size_t size) override {
    if (!owns(owner) || machine_.state() != FirmwareUpdateState::ReceivingStaging) return FirmwareUpdateApiResult::NotReady;
    if (!data || !size || received_ + size > expected_) return FirmwareUpdateApiResult::InvalidRequest;
    received_ += size; return FirmwareUpdateApiResult::Accepted;
  }
  FirmwareUpdateApiResult validate(uint64_t owner) override {
    if (!owns(owner)) return FirmwareUpdateApiResult::PermissionDenied;
    if (received_ != expected_) { machine_.abortRequest(); return FirmwareUpdateApiResult::IncompleteUpload; }
    FirmwareCandidate candidate = candidate_;
    candidate.imageSize = received_; candidate.complete = true;
    if (!machine_.finishStaging(candidate)) return FirmwareUpdateApiResult::InvalidCandidate;
    return FirmwareUpdateApiResult::Accepted;
  }
  FirmwareUpdateApiResult install(uint64_t owner) override {
    if (!owns(owner)) return FirmwareUpdateApiResult::PermissionDenied;
    if (!machine_.beginInstall()) return FirmwareUpdateApiResult::InvalidCandidate;
    reboot_ = true; return FirmwareUpdateApiResult::Accepted;
  }
  void abort(uint64_t owner) override { if (owns(owner)) machine_.abortRequest(); }
  bool owns(uint64_t owner) const override { return owner && owner == owner_; }
  FirmwareUpdateStatusView status() const override {
    FirmwareUpdateStatusView value; value.state = machine_.state(); value.receivedBytes = received_;
    value.expectedBytes = expected_; value.knownGoodVersion = machine_.knownGood().version; value.pendingReboot = reboot_; return value;
  }
  bool consumeRebootRequest() override { const bool result = reboot_; reboot_ = false; return result; }
  void setCandidate(FirmwareCandidate candidate) { candidate_ = candidate; }
  const FirmwareUpdateStateMachine& machine() const { return machine_; }
 private:
  static constexpr size_t Capacity = 1024;
  FirmwareIdentity known_{"plonk-and-play", "esp32-8048s070", "known", 10};
  DefaultFirmwareValidationProvider validator_;
  FirmwareUpdateStateMachine machine_;
  FirmwareCandidate candidate_{{"plonk-and-play", "esp32-8048s070", "candidate", 11}, 0, true, true, true};
  uint64_t owner_ = 0; size_t expected_ = 0; size_t received_ = 0; bool reboot_ = false;
};

static int fail(const char* name) { std::cerr << "FAIL " << name << "\n"; return 1; }

int main() {
  FakeUpdateEndpoint api;
  if (api.begin(7, {false, true, false}, 128) != FirmwareUpdateApiResult::PermissionDenied) return fail("unauthorized");
  if (api.begin(7, {true, false, false}, 128) != FirmwareUpdateApiResult::LifecycleNotSettled) return fail("unsettled");
  if (api.begin(7, {true, true, false}, 0) != FirmwareUpdateApiResult::InvalidRequest) return fail("malformed");
  if (api.begin(7, {true, true, false}, 2048) != FirmwareUpdateApiResult::TooLarge) return fail("too-large");
  if (api.begin(7, {true, true, false}, 128) != FirmwareUpdateApiResult::Accepted) return fail("begin");
  if (api.begin(7, {true, true, false}, 128) != FirmwareUpdateApiResult::AlreadyInProgress) return fail("duplicate");
  uint8_t bytes[64]{};
  if (api.upload(7, bytes, sizeof(bytes)) != FirmwareUpdateApiResult::Accepted) return fail("upload");
  if (api.validate(7) != FirmwareUpdateApiResult::IncompleteUpload) return fail("truncated");
  if (api.machine().knownGood().version != 10 || api.machine().customerDataTouched()) return fail("known-good-after-truncate");

  for (const FirmwareCandidate& invalid : {
      FirmwareCandidate{{"plonk-and-play", "esp32-8048s070", "candidate", 11}, 0, true, true, false},
      FirmwareCandidate{{"other-product", "esp32-8048s070", "candidate", 11}, 0, true, true, true},
      FirmwareCandidate{{"plonk-and-play", "esp32-8048s070", "candidate", 9}, 0, true, true, true},
      FirmwareCandidate{{"plonk-and-play", "esp32-8048s070", "candidate", 11}, 0, true, false, true}}) {
    FakeUpdateEndpoint rejected;
    if (rejected.begin(7, {true, true, false}, 1) != FirmwareUpdateApiResult::Accepted) return fail("invalid-begin");
    rejected.setCandidate(invalid);
    if (rejected.upload(7, bytes, 1) != FirmwareUpdateApiResult::Accepted) return fail("invalid-upload");
    if (rejected.validate(7) != FirmwareUpdateApiResult::InvalidCandidate) return fail("invalid-image-rejection");
    if (rejected.machine().knownGood().version != 10 || rejected.machine().customerDataTouched()) return fail("invalid-known-good");
  }

  if (api.begin(7, {true, true, false}, 64) != FirmwareUpdateApiResult::Accepted) return fail("retry-begin");
  if (api.upload(7, bytes, sizeof(bytes)) != FirmwareUpdateApiResult::Accepted) return fail("complete-upload");
  if (api.validate(7) != FirmwareUpdateApiResult::Accepted) return fail("validate");
  if (api.status().state != FirmwareUpdateState::CandidateReady) return fail("candidate-ready");
  if (api.install(8) != FirmwareUpdateApiResult::PermissionDenied) return fail("install-ownership");
  if (api.install(7) != FirmwareUpdateApiResult::Accepted || !api.consumeRebootRequest()) return fail("install");
  if (api.machine().knownGood().version != 10 || api.machine().customerDataTouched()) return fail("known-good-after-install");
  std::cout << "WP6_UPDATE_API_PASS\n";
}
