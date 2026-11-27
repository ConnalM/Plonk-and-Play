#include "pp/taster_remote_authority.h"

#include <cassert>
#include <cstring>
#include <iostream>

namespace tl = pp::taster_link;

class Link final : public tl::RemoteTasterLinkPort {
 public:
  tl::health::View view() override { return view_; }
  void observeReleased() override { neutral_ = true; }
  bool beginTouch() override {
    if (!view_.mayControl || !neutral_ || touching_) return false;
    neutral_ = false; touching_ = true; return true;
  }
  void cancelTouch() override { neutral_ = false; touching_ = false; ++view_.interactionRevision; }
  bool command(tl::Action action, tl::Mode mode, uint16_t value,
               bool confirmed, uint64_t& commandId) override {
    if (!view_.mayControl || !touching_ ||
        (action == tl::Action::Abandon && !confirmed))
      return false;
    action_ = action; mode_ = mode; value_ = value; confirmed_ = confirmed;
    commandId = ++issued_; touching_ = false; return true;
  }
  bool takeResult(uint64_t commandId, tl::Status& status) override {
    if (!resultId_ || resultId_ != commandId) return false;
    status = result_; resultId_ = 0; return true;
  }
  void acknowledgeUncertainty() override { view_.uncertainOutcome = false; }

  void fresh() {
    view_ = {};
    view_.state = tl::health::State::Fresh;
    view_.hasSnapshot = view_.mayControl = true;
    view_.interactionRevision = 7;
    auto& s = view_.snapshot;
    s.lifecycle = tl::wire::DisplayLifecycle::Ready;
    s.activeMode = s.proposalMode = tl::wire::DisplayMode::Laps;
    s.readiness = tl::wire::Readiness::Ready;
    s.tasterAllowed = true;
    s.proposalLaps = 12; s.proposalMinutes = 10; s.proposalRevision = 3;
    s.entryCount = 2; s.lanes[0].lane = 1; s.lanes[1].lane = 2;
  }
  void deliver(tl::Status status) { result_ = status; resultId_ = issued_; }
  tl::health::View view_{};
  tl::Action action_ = tl::Action::Start;
  tl::Mode mode_ = tl::Mode::None;
  uint16_t value_ = 0;
  bool confirmed_ = false;

 private:
  bool neutral_ = false, touching_ = false;
  uint64_t issued_ = 0, resultId_ = 0;
  tl::Status result_ = tl::Status::Unknown;
};

int main() {
  Link link; link.fresh();
  pp::RemoteTasterAuthority authority(link);
  const auto mapped = authority.snapshot();
  assert(mapped.state.lifecycle == pp::SessionLifecycle::Ready);
  assert(mapped.proposal.lapTarget == 12 && mapped.readiness == pp::StartReadiness::Ready);

  // A genuine neutral sample is required before the first physical down.
  assert(!authority.beginTouch());
  authority.observeReleased();
  assert(authority.beginTouch());
  assert(authority.submitStart(101));
  pp::RequestResult value{}; pp::RequestRejection rejection{};
  assert(!authority.result(101, value, rejection));
  link.deliver(tl::Status::Accepted);
  assert(authority.result(101, value, rejection));
  assert(value == pp::RequestResult::Accepted && rejection == pp::RequestRejection::None);

  // A terminal result and the next neutral/down sequence permit one new
  // command; no snapshot is treated as a fabricated result.
  authority.observeReleased(); assert(authority.beginTouch());
  assert(authority.submitSetup(102, pp::SessionMode::LapRace, 20, 10,
                               pp::LapFinishBehaviour::CompleteFullRaceDistance, 3));
  assert(link.action_ == tl::Action::Setup && link.mode_ == tl::Mode::Laps && link.value_ == 20);
  link.deliver(tl::Status::Conflict);
  assert(authority.result(102, value, rejection));
  assert(value == pp::RequestResult::Rejected &&
         rejection == pp::RequestRejection::ProposalRevisionConflict);

  // A lost/uncertain response is not recast as rejection or acceptance. The
  // local pending indicator is abandoned, then only a new neutral/down can
  // make another command after fresh authority returns.
  authority.observeReleased(); assert(authority.beginTouch());
  assert(authority.submitStart(103));
  link.view_.uncertainOutcome = true;
  assert(!authority.result(103, value, rejection));
  assert(authority.pendingUncertain(103));
  assert(!link.view_.uncertainOutcome);
  link.fresh(); authority.observeReleased(); assert(authority.beginTouch());
  assert(authority.submitOperation(104, pp::SessionOperation::EndRace, true));
  assert(link.action_ == tl::Action::Abandon && link.confirmed_);
  link.deliver(tl::Status::Rejected);
  assert(authority.result(104, value, rejection));
  assert(value == pp::RequestResult::Rejected);

  // The real surface model receives the unknown-outcome signal without a
  // fabricated result, clears only its local spinner, and requires a fresh
  // neutral/down before another request.
  Link modelLink; modelLink.fresh();
  pp::RemoteTasterAuthority modelAuthority(modelLink);
  pp::TasterSurfaceModel model(modelAuthority);
  model.sync();
  modelAuthority.observeReleased(); assert(modelAuthority.beginTouch());
  assert(model.start() && model.setupPending());
  modelLink.view_.uncertainOutcome = true;
  model.service();
  assert(!model.setupPending());
  modelLink.fresh();
  modelAuthority.observeReleased(); assert(modelAuthority.beginTouch());
  assert(model.start());
  std::cout << "Remote Taster authority command/result and gesture contract PASS\n";
}
