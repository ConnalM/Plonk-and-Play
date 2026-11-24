#include <cassert>
#include <iostream>
#include "pp/race_control.h"
#include "pp/published_control_authority.h"

namespace pp {
static Time clockUs=1000000;
Time systemTime(){return clockUs;}
}
using namespace pp;
namespace tl=pp::taster_link;
namespace {
class Owner final : public StartAuthority {
public:
  mutable PublishedControlAuthority published;
  mutable bool intervene=false;
  mutable Time delayAtClaim=0;
  bool authorizesStart(uint64_t id) const override {return published.authorizes(id);}
  bool hasController() const override {return published.hasController();}
  StandaloneAuthorityStamp standaloneStamp() const override {return published.standaloneStamp();}
  bool claimStandalone(uint64_t generation) const override {
    // Simulate HTTPD committing both transitions after gate.take, before
    // the atomic claim. There is no currently connected owner afterwards.
    if(intervene) {
      intervene=false;ControlLease::Snapshot s{};
      s.owner=s.connectedOwner=7;published.publish(s);
      s.connectedOwner=0;published.publish(s);
    }
    const bool claimed=published.claimStandalone(generation);
    clockUs+=delayAtClaim;delayAtClaim=0;
    return claimed;
  }
  void connected(bool value) {
    ControlLease::Snapshot s{};s.owner=7;s.connectedOwner=value?7:0;
    published.publish(s);
  }
};
struct Rig {
  Bus bus;
  Bus::Endpoint rc=bus.attach(Role::RaceControl),engine=bus.attach(Role::RaceEngine),
    browser=bus.attach(Role::Presentation),local=bus.attach(Role::Presentation);
  ActiveSessionDefinition active;
  Owner owner;
  RaceControlModule control{bus,rc,active};
  Rig() {
    for(auto t:{Type::StartRequest,Type::SetupRequest,Type::SessionOperationRequest,
                Type::PauseSettled,Type::CompetitionComplete})
      assert(bus.subscribe(rc,t));
    for(auto t:{Type::GoScheduled,Type::SessionOperation}) assert(bus.subscribe(engine,t));
    assert(bus.subscribe(browser,Type::RequestResult));
    assert(bus.subscribe(local,Type::RequestResult));
    control.setControllerStartAuthority(browser,owner);
    control.setStandaloneTasterEndpoint(local);
    ProposedRaceSetup s{};
    s.activeLanes=2;s.mode=SessionMode::LapRace;s.lapTarget=10;s.durationMinutes=5;
    s.finish=LapFinishBehaviour::CompleteFullRaceDistance;
    s.entries[0].startFinish={InputIdentity(1,1),1,InputRole::StartFinish};
    s.entries[1].startFinish={InputIdentity(2,1),2,InputRole::StartFinish};
    s.entries[0].mugId=1;s.entries[1].mugId=2;
    control.setProposedRaceSetup(s);control.setRequiredCapabilityAvailable(true);
    assert(control.readiness()==StartReadiness::Ready);
  }
  tl::Command command(uint64_t id,tl::Action action) {
    const auto c=control.remoteTasterContext();
    tl::Command q{};q.session=c.session;q.id=id;q.authorityGeneration=c.authorityGeneration;
    q.lifecycleRevision=c.lifecycleRevision;q.raceId=c.raceId;q.proposalRevision=c.proposalRevision;
    q.expiresAtUs=clockUs+100000;q.action=action;return q;
  }
  tl::Status submit(const tl::Command& q){return control.submitRemoteTaster(q,clockUs);}
  tl::Status status(const tl::Command& q){return control.remoteTasterResult(q.session,q.id);}
  void drainEngine(){Message m;while(bus.receive(engine,m)){}}
  void go(){clockUs=control.scheduledGo();control.tick(clockUs);drainEngine();assert(control.state()==SessionLifecycle::Racing);}
  void fact(Type t){Message m{};m.type=t;m.relevantTime=clockUs;
    assert(bus.publish(engine,m)==Delivery::Delivered);control.tick(clockUs);}
  void accepted(const tl::Command& q){
    assert(submit(q)==tl::Status::Queued);assert(control.executeRemoteTaster());
    assert(status(q)==tl::Status::Accepted);assert(submit(q)==tl::Status::Accepted);
    assert(!control.executeRemoteTaster());drainEngine();
    assert(!bus.queued(browser) && !bus.queued(local)); // no broadcast/correlation collision
  }
};
}
int main(){
  constexpr tl::Session A{1,1},B{1,2};
  Rig r;
  assert(!r.control.remoteTasterContext().tasterAllowed);
  assert(r.control.openRemoteTaster(A));
  assert(!r.control.openRemoteTaster(B));
  assert(r.control.remoteTasterContext().tasterAllowed);
  // Actual committed authority generation; an unrelated pending request
  // publication does not falsely invalidate an otherwise unchanged lease.
  auto stamp=r.owner.standaloneStamp();
  ControlLease::Snapshot pending{};pending.requester=8;pending.requestId=1;
  r.owner.published.publish(pending);
  assert(r.owner.standaloneStamp().generation==stamp.generation);
  auto q=r.command(1,tl::Action::Start);
  assert(r.submit(q)==tl::Status::Queued);
  r.owner.connected(true);r.owner.connected(false);
  assert(r.control.remoteTasterContext().tasterAllowed);
  assert(!r.control.executeRemoteTaster() && r.status(q)==tl::Status::Stale);
  assert(!r.active.current() && r.control.state()==SessionLifecycle::Ready);
  // The same invalidation at the final atomic claim, not just dequeue.
  q=r.command(2,tl::Action::Start);
  assert(r.submit(q)==tl::Status::Queued);r.owner.intervene=true;
  assert(!r.control.executeRemoteTaster() && r.status(q)==tl::Status::Rejected);
  assert(!r.active.current());
  // Transport suspend/resume cannot revive a pre-transition command.
  q=r.command(3,tl::Action::Start);assert(r.submit(q)==tl::Status::Queued);
  r.owner.published.suspendTransport();r.owner.published.resumeTransport();
  assert(!r.control.executeRemoteTaster() && r.status(q)==tl::Status::Stale);
  // Expiry, proposal/revision changes and fresh session isolation.
  q=r.command(4,tl::Action::Start);assert(r.submit(q)==tl::Status::Queued);
  clockUs=q.expiresAtUs;
  assert(!r.control.executeRemoteTaster() && r.status(q)==tl::Status::Expired);
  q=r.command(5,tl::Action::Start);assert(r.submit(q)==tl::Status::Queued);
  auto replacement=*r.control.proposedRaceSetup();replacement.lapTarget=11;
  r.control.setProposedRaceSetup(replacement);
  assert(!r.control.executeRemoteTaster() && r.status(q)==tl::Status::Stale);
  q=r.command(6,tl::Action::Start);assert(r.submit(q)==tl::Status::Queued);
  r.control.setRequiredCapabilityAvailable(false);r.control.setRequiredCapabilityAvailable(true);
  assert(!r.control.executeRemoteTaster() && r.status(q)==tl::Status::Stale);
  q=r.command(7,tl::Action::Start);assert(r.submit(q)==tl::Status::Queued);
  r.control.closeRemoteTaster();
  assert(!r.control.executeRemoteTaster() && r.status(q)==tl::Status::WrongSession);
  assert(!r.control.openRemoteTaster(A));assert(r.control.openRemoteTaster(B));
  assert(r.submit(q)==tl::Status::WrongSession);
  // Time may advance while obtaining the final claim. It must not let an
  // expired command execute merely because dequeue preceded the deadline.
  q=r.command(1,tl::Action::Start);assert(r.submit(q)==tl::Status::Queued);
  r.owner.delayAtClaim=100000;
  assert(!r.control.executeRemoteTaster() && r.status(q)==tl::Status::Rejected);
  assert(!r.active.current());
  // Use a fresh link before restarting command IDs.
  r.control.closeRemoteTaster();assert(r.control.openRemoteTaster({1,3}));
  // Supported setup preserves inactive values/identities; no administrative
  // native message fields arrive from the network.
  q=r.command(1,tl::Action::Setup);q.mode=tl::Mode::Timed;q.value=8;r.accepted(q);
  assert(r.control.proposedRaceSetup()->mode==SessionMode::Endurance);
  assert(r.control.proposedRaceSetup()->lapTarget==11 && r.control.proposedRaceSetup()->durationMinutes==8);
  assert(r.control.proposedRaceSetup()->finish==LapFinishBehaviour::CompleteCurrentLap);
  q=r.command(2,tl::Action::Setup);q.mode=tl::Mode::Laps;q.value=12;r.accepted(q);
  assert(r.control.proposedRaceSetup()->durationMinutes==8 && r.control.proposedRaceSetup()->lapTarget==12);
  assert(r.control.proposedRaceSetup()->entries[0].mugId==1 && r.control.proposedRaceSetup()->entries[1].mugId==2);
  assert(r.control.proposedRaceSetup()->finish==LapFinishBehaviour::CompleteFullRaceDistance);
  q=r.command(3,static_cast<tl::Action>(99));assert(r.submit(q)==tl::Status::Invalid);
  q=r.command(4,tl::Action::Start);r.accepted(q);
  assert(r.active.current() && !r.active.current()->officialRecordEligible());
  assert(r.control.state()==SessionLifecycle::Starting);
  auto race=r.control.remoteTasterContext().raceId;assert(race!=0);
  r.go();
  q=r.command(5,tl::Action::Pause);r.accepted(q);
  assert(r.control.state()==SessionLifecycle::Paused);
  q=r.command(6,tl::Action::Resume);assert(r.submit(q)==tl::Status::Queued);
  assert(r.control.executeRemoteTaster() && r.status(q)==tl::Status::Rejected);
  assert(r.control.state()==SessionLifecycle::Paused); // native settlement fence
  r.fact(Type::PauseSettled);
  q=r.command(7,tl::Action::Resume);r.accepted(q);r.go();
  q=r.command(8,tl::Action::Pause);r.accepted(q);r.fact(Type::PauseSettled);
  q=r.command(9,tl::Action::Abandon);assert(r.submit(q)==tl::Status::Invalid);
  q=r.command(10,tl::Action::Abandon);q.confirmed=true;r.accepted(q);
  assert(r.control.state()==SessionLifecycle::Ready && !r.active.current());
  q=r.command(11,tl::Action::Start);r.accepted(q);
  assert(r.control.remoteTasterContext().raceId!=race);
  r.go();r.fact(Type::CompetitionComplete);
  q=r.command(12,tl::Action::RaceAgain);r.accepted(q);
  assert(r.control.state()==SessionLifecycle::Ready);
  assert(r.control.proposedRaceSetup()->lapTarget==12);
  // Practice is display-only for both ready and active modes.
  replacement=*r.control.proposedRaceSetup();replacement.mode=SessionMode::OpenPractice;
  r.control.setProposedRaceSetup(replacement);
  assert(!r.control.remoteTasterContext().tasterAllowed);
  q=r.command(13,tl::Action::Start);assert(r.submit(q)==tl::Status::Forbidden);
  // Existing Browser START remains official and retains its ordinary
  // correlated result. Remote status does not consume or forge it.
  replacement.mode=SessionMode::LapRace;r.control.setProposedRaceSetup(replacement);
  r.owner.connected(true);
  Message start{};start.type=Type::StartRequest;start.correlation=1;
  start.clientContext=ClientContext::RaceDirectorSmug;start.controlOwner=7;
  start.sessionMode=SessionMode::None;
  assert(r.bus.publish(r.browser,start)==Delivery::Delivered);
  r.control.tick(clockUs);
  assert(r.active.current() && r.active.current()->officialRecordEligible());
  Message result{};assert(r.bus.receive(r.browser,result));
  assert(result.type==Type::RequestResult && result.correlation==1 && result.requestResult==RequestResult::Accepted);
  assert(!r.control.remoteTasterContext().tasterAllowed);
  assert(r.status(q)==tl::Status::Forbidden);
  // Exercise the actual Browser SetupRequest path for a shared LAPS target
  // beyond the fixed Taster limit. No direct fixture mutation of the target.
  Rig limit;
  limit.owner.connected(true);
  Message large{};large.type=Type::SetupRequest;large.correlation=101;
  large.clientContext=ClientContext::RaceDirectorSmug;large.controlOwner=7;
  large.sessionMode=SessionMode::LapRace;large.setupLapTarget=1000;
  large.setupDurationMinutes=5;
  large.setupFinishPolicy=static_cast<uint8_t>(LapFinishBehaviour::CompleteFullRaceDistance);
  large.proposalRevision=limit.control.proposalRevision();
  assert(limit.bus.publish(limit.browser,large)==Delivery::Delivered);
  limit.control.tick(clockUs);
  for(auto ep:{limit.browser,limit.local}) {
    Message reply{};assert(limit.bus.receive(ep,reply));
    assert(reply.type==Type::RequestResult && reply.correlation==101 &&
           reply.requestResult==RequestResult::Accepted);
  }
  assert(limit.control.proposedRaceSetup()->lapTarget==1000);
  const auto savedRevision=limit.control.proposalRevision();
  limit.owner.connected(false);
  assert(limit.control.openRemoteTaster({2,1}));
  assert(limit.control.remoteTasterContext().tasterAllowed);
  auto tooLarge=limit.command(1,tl::Action::Start);
  assert(limit.submit(tooLarge)==tl::Status::Queued);
  assert(limit.control.executeRemoteTaster() && limit.status(tooLarge)==tl::Status::Rejected);
  assert(limit.control.state()==SessionLifecycle::Ready && !limit.active.current());
  assert(!limit.control.startCommitted());
  assert(limit.control.proposedRaceSetup()->lapTarget==1000);
  assert(limit.control.proposalRevision()==savedRevision);
  assert(!limit.bus.queued(limit.engine) && !limit.bus.queued(limit.browser) &&
         !limit.bus.queued(limit.local));
  // Explicit Taster adjustment is still permitted, and 999 is inclusive.
  auto adjust=limit.command(2,tl::Action::Setup);
  adjust.mode=tl::Mode::Laps;adjust.value=999;limit.accepted(adjust);
  assert(limit.control.proposedRaceSetup()->lapTarget==999);
  auto within=limit.command(3,tl::Action::Start);limit.accepted(within);
  assert(limit.active.current() && limit.active.current()->lapTarget()==999 &&
         !limit.active.current()->officialRecordEligible());
  // TIMED ignores the inactive shared lap target; this is a LAPS START
  // restriction, not a reason to clamp values or reject another mode.
  Rig timed;
  auto tSetup=*timed.control.proposedRaceSetup();tSetup.lapTarget=1000;
  timed.control.setProposedRaceSetup(tSetup);
  assert(timed.control.openRemoteTaster({3,1}));
  auto timedAdjust=timed.command(1,tl::Action::Setup);
  timedAdjust.mode=tl::Mode::Timed;timedAdjust.value=5;timed.accepted(timedAdjust);
  assert(timed.control.proposedRaceSetup()->lapTarget==1000);
  timed.accepted(timed.command(2,tl::Action::Start));
  assert(timed.active.current() && timed.active.current()->mode()==SessionMode::Endurance &&
         !timed.active.current()->officialRecordEligible());
  // The ordinary Browser handler must still accept 1000 laps and return its
  // official correlated result, even with the remote session open.
  Rig browserLarge;
  browserLarge.owner.connected(true);
  assert(browserLarge.control.openRemoteTaster({4,1}));
  large.proposalRevision=browserLarge.control.proposalRevision();
  assert(browserLarge.bus.publish(browserLarge.browser,large)==Delivery::Delivered);
  browserLarge.control.tick(clockUs);
  for(auto ep:{browserLarge.browser,browserLarge.local}) {
    Message reply{};assert(browserLarge.bus.receive(ep,reply));
    assert(reply.type==Type::RequestResult && reply.correlation==101 &&
           reply.requestResult==RequestResult::Accepted);
  }
  assert(browserLarge.control.proposedRaceSetup()->lapTarget==1000);
  assert(browserLarge.bus.publish(browserLarge.browser,start)==Delivery::Delivered);
  browserLarge.control.tick(clockUs);
  assert(browserLarge.active.current() && browserLarge.active.current()->lapTarget()==1000 &&
         browserLarge.active.current()->officialRecordEligible());
  assert(browserLarge.bus.receive(browserLarge.browser,result));
  assert(result.type==Type::RequestResult && result.correlation==1 &&
         result.requestResult==RequestResult::Accepted);
  std::cout<<"S3f actual Race Control remote execution and authority generation PASS\n";
}
