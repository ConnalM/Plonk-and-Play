#include "pp/taster_link_health.h"
#include <cassert>
#include <iostream>

namespace h=pp::taster_link::health;
namespace w=pp::taster_link::wire;
using namespace pp::taster_link;
constexpr Session A{1,1}, B{1,2}, C{2,1};
// Exact no-drift fixture: Core clock is Taster clock + 1000000 us.
// One-way delays are 1000 us and Core processing time is 200 us.
static void ping(h::Client& client,Session s,uint64_t t) {
  w::Ping q;
  assert(client.startPing(t,q));
  assert(client.acceptPong(s,{q.token,q.tasterSendUs,t+1001000,t+1001200},t+2200));
}
static w::DisplaySnapshot snapshot(Session s,uint64_t t) {
  w::DisplaySnapshot v{};
  v.session=s;v.coreTimeUs=t+1000000;v.lifecycle=w::DisplayLifecycle::Ready;
  v.activeMode=v.proposalMode=w::DisplayMode::Laps;
  v.readiness=w::Readiness::Ready;v.tasterAllowed=true;
  v.authorityGeneration=1;v.lifecycleRevision=1;v.proposalRevision=7;
  v.proposalLaps=12;return v;
}
static void establish(h::Client& c,Session s,uint64_t t) {
  assert(c.open(s,t));assert(c.view(t).state==h::State::Synchronising);
  assert(!c.acceptSnapshot(snapshot(s,t),t));
  assert(!c.beginTouch(t));
  ping(c,s,t);
  assert(c.acceptSnapshot(snapshot(s,t+2200),t+2200));
  assert(c.view(t+2200).state==h::State::Fresh);
  assert(c.view(t+2200).mayControl && !c.view(t+2200).readyForNewTouch);
}
int main() {
  h::Clock clock;h::Bounds b;
  assert(!clock.at(0,b));
  assert(clock.update(100,2300,1001100,1001300));
  assert(clock.at(2300,b) && b.lower==1001300 && b.upper==1003301);
  assert(clock.at(102300,b) && b.lower<=1102300 && b.upper>=1102300);
  assert(!clock.at(2299,b));
  assert(!clock.at(2300+h::Policy::MappingAgeUs,b));
  assert(!clock.update(10,9,1,2));
  assert(!clock.update(10,20,5,4));
  assert(!clock.update(10,20,5,50));
  assert(!clock.update(0,h::Policy::PingTimeoutUs,10,11));
  assert(!clock.update(0,10,UINT64_MAX-1,UINT64_MAX));
  assert(clock.update(10,10,100,100));
  assert(clock.at(10,b) && b.lower==100 && b.upper==100);
  // Very asymmetric delays: true Core now is near either end of the
  // interval. Neither case assumes half the round-trip latency.
  assert(clock.update(1000,11000,1001001,1001201));
  assert(clock.at(11000,b) && b.lower<=1011000 && b.upper>=1011000);
  assert(clock.update(1000,11000,1010799,1010999));
  assert(clock.at(11000,b) && b.lower<=1011000 && b.upper>=1011000);
  // Negative offset is just as valid; no signed offset overflow.
  assert(clock.update(10000000,10002200,1000,1200));
  assert(clock.at(10002200,b) && b.lower==1200 && b.upper==3201);

  h::Client c;establish(c,A,0);
  Context ctx{};
  assert(!c.beginTouch(2200));
  c.observeReleased(2200);assert(c.beginTouch(2200));
  auto held=c.view(2200);assert(held.gestureActive);
  // Browser takeover invalidates a held gesture, even if authority returns
  // before physical release. Fresh frame does not mean fresh touch.
  auto s=snapshot(A,3000);s.authorityGeneration=2;s.tasterAllowed=false;
  assert(c.acceptSnapshot(s,3000));assert(!c.view(3000).mayControl);
  assert(c.view(3000).interactionRevision!=held.interactionRevision);
  s.coreTimeUs=1004000;s.authorityGeneration=3;s.tasterAllowed=true;
  assert(c.acceptSnapshot(s,4000));
  assert(!c.releaseForCommand(4000,1,ctx) && !c.beginTouch(4000));
  c.observeReleased(4000);assert(c.beginTouch(4000));
  assert(c.releaseForCommand(4100,1,ctx));
  assert(ctx.session==A && ctx.authorityGeneration==3 && ctx.proposalRevision==7);
  assert(c.pending() && !c.view(4100).mayControl);
  assert(!c.acceptResult(B,{1,Status::Accepted},4200));
  assert(!c.acceptResult(A,{2,Status::Accepted},4200));
  assert(c.acceptResult(A,{1,Status::InFlight},4200) && c.pending());
  assert(c.acceptResult(A,{1,Status::Accepted},4300) && !c.pending());
  assert(c.view(4300).snapshot.lifecycle==w::DisplayLifecycle::Ready);
  assert(c.view(4300).awaitingPostResultSnapshot && !c.view(4300).mayControl);
  c.observeReleased(4300);assert(!c.beginTouch(4300));
  assert(!c.releaseForCommand(4300,2,ctx));
  s=snapshot(A,6000);s.authorityGeneration=3;
  assert(c.acceptSnapshot(s,6000));
  assert(c.view(6000).mayControl && !c.view(6000).readyForNewTouch);
  c.observeReleased(6000);assert(c.beginTouch(6000));
  assert(!c.releaseForCommand(6000,1,ctx)); // no replay
  c.observeReleased(6100);assert(c.beginTouch(6100));
  assert(c.releaseForCommand(6100,2,ctx));
  c.disconnect();auto lost=c.view(6500);
  assert(lost.state==h::State::Stale && lost.hasSnapshot && !lost.mayControl &&
    !lost.snapshot.tasterAllowed && lost.uncertainOutcome);
  assert(!c.pending() && !c.releaseForCommand(6500,3,ctx));
  assert(!c.open(A,6600)); // same identity never resumes
  establish(c,B,7000);
  assert(c.view(9200).uncertainOutcome && !c.beginTouch(9200));
  c.acknowledgeUncertainty();assert(!c.view(9200).uncertainOutcome);
  c.observeReleased(9200);assert(c.beginTouch(9200));
  assert(c.releaseForCommand(9200,1,ctx) && ctx.session==B);
  // Ownership change cancels pending knowledge, never replays it.
  s=snapshot(B,10000);s.authorityGeneration=2;s.tasterAllowed=false;
  assert(c.acceptSnapshot(s,10000) && !c.pending() && c.view(10000).uncertainOutcome);
  assert(!c.acceptResult(B,{1,Status::Accepted},10000));
  c.disconnect();assert(c.open(C,11000));
  assert(!c.view(11000).hasSnapshot); // different Core boot: no old race

  // Both successful and rejected commands may leave an obsolete READY
  // snapshot. All terminal statuses use the same post-result barrier.
  for(const auto status:{Status::Accepted,Status::Rejected,Status::Unknown,
                         Status::WrongSession,Status::Stale,Status::Expired,
                         Status::Forbidden,Status::Invalid,Status::OldId,
                         Status::Conflict,Status::Busy}) {
    h::Client barrier;establish(barrier,A,0);
    barrier.observeReleased(2200);assert(barrier.beginTouch(2200));
    assert(barrier.releaseForCommand(2300,1,ctx));
    assert(!barrier.view(2300).snapshot.tasterAllowed);
    // Even a sample newer than the command sample may precede execution.
    auto queued=snapshot(A,3000);
    assert(barrier.acceptSnapshot(queued,3000));
    assert(barrier.acceptResult(A,{1,status},4000) && !barrier.pending());
    const auto bound=barrier.view(4000).coreNow.upper;
    auto blocked=[&](uint64_t t) {
      auto v=barrier.view(t);
      assert(v.state==h::State::Fresh && v.awaitingPostResultSnapshot &&
             !v.mayControl && !v.snapshot.tasterAllowed &&
             !v.readyForNewTouch && !v.gestureActive);
      barrier.observeReleased(t);assert(!barrier.beginTouch(t));
      assert(!barrier.releaseForCommand(t,2,ctx));
    };
    blocked(4000);
    assert(barrier.acceptSnapshot(queued,4100)); // same sample time
    blocked(4100);
    queued.coreTimeUs=1004100; // newer but not provably after result receipt
    assert(barrier.acceptSnapshot(queued,4200));
    blocked(4200);
    queued.coreTimeUs=bound;
    assert(barrier.acceptSnapshot(queued,6000)); // exact threshold not enough
    blocked(6000);
    queued.coreTimeUs=bound+1;
    assert(barrier.acceptSnapshot(queued,6100));
    assert(!barrier.view(6100).awaitingPostResultSnapshot &&
           barrier.view(6100).mayControl && barrier.view(6100).snapshot.tasterAllowed);
    assert(!barrier.beginTouch(6100)); // earlier neutral reports do not count
    barrier.observeReleased(6200);assert(barrier.beginTouch(6200));
    assert(barrier.releaseForCommand(6200,2,ctx));
  }
  // A new sample may instead show authority lost; it must not grant control.
  h::Client spectator;establish(spectator,A,0);
  spectator.observeReleased(2200);assert(spectator.beginTouch(2200));
  assert(spectator.releaseForCommand(2300,1,ctx));
  assert(spectator.acceptResult(A,{1,Status::Accepted},4000));
  auto updated=snapshot(A,6000);updated.tasterAllowed=false;
  ++updated.authorityGeneration;
  assert(spectator.acceptSnapshot(updated,6000));
  assert(!spectator.view(6000).awaitingPostResultSnapshot &&
         !spectator.view(6000).mayControl);
  spectator.observeReleased(6000);assert(!spectator.beginTouch(6000));

  // Fresh GO is sampled, not invented. Once its possible validity expires,
  // suppress it even before the snapshot becomes stale.
  h::Client lights;establish(lights,A,0);
  s=snapshot(A,3000);s.lifecycle=w::DisplayLifecycle::Racing;
  s.phase=w::Phase::Go;s.flags=w::Green;s.phaseUntilUs=1006000;
  s.elapsedUs=1234;s.redCount=5;
  assert(lights.acceptSnapshot(s,3000));
  assert(lights.view(3000).snapshot.flags&w::Green);
  auto late=lights.view(6000);
  assert(late.state==h::State::Fresh && !(late.snapshot.flags&w::Green));
  assert(late.snapshot.elapsedUs==1234);
  s=snapshot(A,7000);s.lifecycle=w::DisplayLifecycle::Starting;
  s.phase=w::Phase::Countdown;s.redCount=5;s.redLit=4;s.phaseUntilUs=1008000;
  assert(lights.acceptSnapshot(s,7000));
  assert(!(lights.view(9000).snapshot.flags&w::Green));
  auto stale=lights.view(507000);
  assert(stale.state==h::State::Stale && stale.snapshot.redLit==0 &&
         stale.snapshot.phase==w::Phase::None && !stale.mayControl);
  assert(!lights.acceptSnapshot(snapshot(A,507001),507001)); // fresh link required

  // A valid pong alone cannot keep stale race data current.
  h::Client noDisplay;establish(noDisplay,A,0);
  ping(noDisplay,A,200000);
  assert(noDisplay.view(502200).state==h::State::Stale);
  // A new clock observation must agree with the earlier mapping.
  h::Client jump;establish(jump,A,0);
  w::Ping refresh{};
  assert(jump.startPing(100000,refresh));
  assert(!jump.acceptPong(A,{refresh.token,refresh.tasterSendUs,1191000,1191200},102200));
  assert(!jump.connected() && jump.view(102200).state==h::State::Stale);
  // A timely refreshed mapping permits ongoing display beyond the first
  // mapping lifetime, without refreshing any old snapshot.
  h::Client renewed;establish(renewed,A,0);
  for(uint64_t t=200000;t<=4600000;t+=200000)
    assert(renewed.acceptSnapshot(snapshot(A,t),t));
  ping(renewed,A,4700000);
  assert(renewed.acceptSnapshot(snapshot(A,4800000),4800000));
  assert(renewed.acceptSnapshot(snapshot(A,5000000),5000000));
  assert(renewed.view(5100000).state==h::State::Fresh);

  // Delayed/future/out-of-order snapshots cannot revive or replace current data.
  h::Client delayed;assert(delayed.open(A,0));ping(delayed,A,0);
  auto old=snapshot(A,0);
  assert(!delayed.acceptSnapshot(old,2200) && !delayed.connected());
  h::Client future;establish(future,A,0);
  assert(!future.acceptSnapshot(snapshot(A,500000),3000) && !future.connected());
  h::Client backward;establish(backward,A,0);
  auto earlier=snapshot(A,2000);
  assert(!backward.acceptSnapshot(earlier,3000) && !backward.connected());
  h::Client wrongSession;establish(wrongSession,A,0);
  assert(!wrongSession.acceptSnapshot(snapshot(B,3000),3000) && !wrongSession.connected());
  h::Client time;establish(time,A,0);
  assert(!time.tick(2199) && !time.connected());
  // Ping challenge matching and startup/ping deadlines.
  h::Client start;assert(start.open(A,0));
  assert(!start.tick(h::Policy::StartupUs));
  h::Client missing;assert(missing.open(A,0));w::Ping q{};
  assert(missing.startPing(0,q));assert(!missing.startPing(1,q));
  assert(!missing.tick(h::Policy::PingTimeoutUs));
  h::Client bad;assert(bad.open(A,0));assert(bad.startPing(0,q));
  assert(!bad.acceptPong(A,{q.token+1,0,1001000,1001200},2200) && !bad.connected());
  h::Client wrongEcho;assert(wrongEcho.open(A,0));assert(wrongEcho.startPing(0,q));
  assert(!wrongEcho.acceptPong(A,{q.token,1,1001000,1001200},2200));
  h::Client wrongPongSession;assert(wrongPongSession.open(A,0));
  assert(wrongPongSession.startPing(0,q));
  assert(!wrongPongSession.acceptPong(B,{q.token,0,1001000,1001200},2200));
  // Pending timeout despite continuously fresh snapshots; no auto retry.
  h::Client pending;establish(pending,A,0);
  pending.observeReleased(2200);assert(pending.beginTouch(2200));
  assert(pending.releaseForCommand(2200,1,ctx));
  for(uint64_t t=200000;t<=2000000;t+=200000)
    assert(pending.acceptSnapshot(snapshot(A,t),t));
  assert(!pending.tick(2002200));
  assert(pending.view(2002200).uncertainOutcome && !pending.pending());
  // Repeated valid display frames do not extend the clock mapping lifetime.
  h::Client expired;establish(expired,A,0);
  for(uint64_t t=200000;t<=5000000;t+=200000)
    assert(expired.acceptSnapshot(snapshot(A,t),t));
  assert(!expired.tick(5002200));
  // A proposal revision change cancels preview without altering actual race.
  h::Client preview;establish(preview,A,0);preview.observeReleased(2200);
  assert(preview.beginTouch(2200));auto rev=preview.view(2200).interactionRevision;
  s=snapshot(A,3000);++s.proposalRevision;
  assert(preview.acceptSnapshot(s,3000));
  assert(preview.view(3000).interactionRevision!=rev && !preview.view(3000).gestureActive);
  assert(!preview.releaseForCommand(3000,1,ctx));
  // Unsupported Practice remains visible only with no local authority.
  h::Client practice;establish(practice,A,0);
  s=snapshot(A,3000);s.proposalMode=w::DisplayMode::Practice;s.tasterAllowed=false;
  assert(practice.acceptSnapshot(s,3000));
  practice.observeReleased(3000);assert(!practice.beginTouch(3000));
  assert(practice.view(3000).snapshot.proposalMode==w::DisplayMode::Practice);
  std::cout<<"S3e clock bounds, freshness, touch invalidation and recovery PASS\n";
}
