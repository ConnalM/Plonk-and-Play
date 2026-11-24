#include "pp/taster_link_wire.h"
#include "pp/taster_link_display_projection.h"
#include <openssl/hmac.h>
#include <openssl/crypto.h>
#include <cassert>
#include <iostream>

namespace w=pp::taster_link::wire;
using namespace pp;
using namespace pp::taster_link;

class TestMac final : public w::Mac {
public:
  bool sign(const uint8_t* b,size_t n,uint8_t* tag) const override {
    const uint8_t key[32]={42}; unsigned length=0;
    return HMAC(EVP_sha256(),key,sizeof(key),b,n,tag,&length)!=nullptr &&
           length==w::TagSize;
  }
  bool verify(const uint8_t* b,size_t n,const uint8_t* tag) const override {
    uint8_t expected[w::TagSize]{};
    return sign(b,n,expected) && CRYPTO_memcmp(tag,expected,sizeof(expected))==0;
  }
};
static w::Read feed(w::StreamDecoder& d,const w::Packet& p,size_t a,size_t b,w::Frame& out) {
  auto r=w::Read::NeedMore;
  for(size_t i=a;i<b;++i) {
    r=d.push(p.bytes[i],out);
    if(r!=w::Read::NeedMore) {
      if(r==w::Read::Ready) assert(i+1==b);
      break;
    }
  }
  return r;
}
int main() {
  static_assert(w::DisplayPayloadSize==322 && w::MaxFrame==386);
  const Session session{12,34};
  Context c{session,5,9,42,7,Lifecycle::Racing,true};
  TasterSnapshot sample{};
  auto& n=sample.state;
  sample.proposalRevision=7; n.proposalRevision=7;
  sample.readiness=StartReadiness::LifecycleNotStartable;
  sample.proposal.activeLanes=2; sample.proposal.lapTarget=1200;
  sample.proposal.durationMinutes=19; sample.proposal.mode=SessionMode::Endurance;
  sample.proposal.finish=LapFinishBehaviour::CompleteCurrentLap;
  n.lifecycle=SessionLifecycle::Racing; n.sessionMode=SessionMode::Endurance;
  n.lapTarget=1200; n.durationMinutes=19; n.finishBehaviour=LapFinishBehaviour::CompleteCurrentLap;
  n.raceElapsed=0x100000002ULL; n.remainingDuration=987654321; n.overtime=1234567;
  n.scheduledGo=100000; n.startPhaseUntil=200000; n.pauseEffectiveAt=300000;
  n.scheduledRestartAt=400000; n.durationExpiryAt=500000; n.finishDisplayUntil=600000;
  n.durationExpired=true; n.simulatedSession=true; n.resultValid=false;
  n.raceIntegrityFaulted=true; n.persistenceFault=true; n.entryCount=2;
  n.redLightCount=5; n.redLightsLit=0; n.startSignal=1;
  n.entries[0].lane=2; n.entries[0].laps=23; n.entries[0].classifiedLaps=22;
  n.entries[0].lapPenalty=1; n.entries[0].position=2; n.entries[0].lapsBehind=1;
  n.entries[0].gapKind=RaceEngineModule::LiveGapKind::Laps; n.entries[0].hasLap=true;
  n.entries[0].lastLapTime=1100000; n.entries[0].bestLapTime=1000000;
  n.entries[0].completed=true; n.entries[0].finishDuration=9999000;
  n.entries[0].hasOvertimeOffset=true; n.entries[0].overtimeOffset=100;
  n.entries[1].lane=1; n.entries[1].laps=23; n.entries[1].classifiedLaps=23;
  n.entries[1].position=1; n.entries[1].waitingForTimingOrigin=true;
  // Unknown gap stays unknown, not an invented zero time gap.
  n.entries[1].gapKind=RaceEngineModule::LiveGapKind::None;
  TasterPresentationPreferences prefs{};
  std::strcpy(prefs.lane1Label,"MGB"); std::strcpy(prefs.lane2Label,"");
  prefs.lane1Graphic=TasterCarGraphic::RedEtype;
  prefs.lane2Graphic=TasterCarGraphic::GreenMgb;
  w::DisplaySnapshot s{};
  assert(w::projectDisplay(sample,&prefs,3,c,777777,s));
  assert(s.proposalLaps==1200 && s.activeLaps==1200 && s.proposalMinutes==19);
  assert(s.activeMode==w::DisplayMode::Timed && s.activeFinish==w::FinishPolicy::CurrentLap);
  assert(s.lanes[0].lane==2 && s.lanes[0].classified==22 && s.lanes[0].penalty==1);
  assert(s.lanes[0].gap==w::Gap::Laps && s.lanes[1].gap==w::Gap::None);
  assert(s.lanes[0].flags==(w::HasLap|w::Completed|w::HasOvertime));
  assert(s.lanes[1].flags==w::WaitingOrigin);
  assert(s.flags==(w::DurationExpired|w::Simulated|w::IntegrityFault|w::PersistenceFault|w::PreferencesPresent));
  assert(s.preferencesRevision==3 && s.graphics[0]==w::Graphic::RedEtype &&
         !std::strcmp(s.labels[0].data(),"MGB") && !s.labels[1][0]);
  const auto ctx=w::commandContext(s);
  assert(ctx.session==c.session && ctx.authorityGeneration==c.authorityGeneration &&
         ctx.lifecycleRevision==c.lifecycleRevision && ctx.raceId==c.raceId &&
         ctx.proposalRevision==c.proposalRevision && ctx.lifecycle==c.lifecycle &&
         ctx.tasterAllowed==c.tasterAllowed);
  const auto frame=w::displayState(s,1);
  TestMac mac; w::Packet packet;
  assert(w::seal(frame,w::Direction::ToTaster,mac,packet));
  assert(packet.length==386);
  assert(packet.bytes[5]==6 && packet.bytes[6]==1 && packet.bytes[7]==66);
  for(size_t split=1;split<packet.length;++split) {
    w::StreamDecoder d(session,w::Direction::ToTaster,mac);
    w::Frame out{}; out.sequence=99;
    assert(feed(d,packet,0,split,out)==w::Read::NeedMore && out.sequence==99);
    assert(feed(d,packet,split,packet.length,out)==w::Read::Ready);
    w::DisplaySnapshot decoded{};
    assert(w::readDisplayState(out,decoded));
    // Exact re-encoding verifies every field and the canonical padding.
    const auto again=w::displayState(decoded,1);
    assert(again.length==frame.length && again.payload==frame.payload);
    assert(decoded.elapsedUs==0x100000002ULL && decoded.lanes[0].finishUs==9999000 &&
           decoded.coreTimeUs==777777 && decoded.phaseUntilUs==200000);
  }
  w::Packet no;
  assert(!w::seal(frame,w::Direction::ToCore,mac,no));
  auto reject=[&](size_t off,uint8_t value) {
    auto bad=packet; bad.bytes[w::HeaderSize+off]=value;
    assert(mac.sign(bad.bytes.data(),bad.length-w::TagSize,bad.bytes.data()+bad.length-w::TagSize));
    w::StreamDecoder d(session,w::Direction::ToTaster,mac);
    w::Frame f{}; f.sequence=123;
    assert(feed(d,bad,0,bad.length,f)==w::Read::Fatal && f.sequence==123);
  };
  reject(36,255); // lifecycle
  reject(37,2); // noncanonical bool
  reject(48,3); // entry count
  reject(144,2); // invalid graphic
  reject(145,1); // invalid label
  reject(149,65); // nonzero padding after NUL
  reject(196,0); // invalid lane
  reject(197,128); // reserved lane flag
  reject(198,255); // unknown gap
  reject(259,2); // duplicate physical lane
  auto corrupt=packet; corrupt.bytes[300]^=1;
  w::StreamDecoder badTag(session,w::Direction::ToTaster,mac); w::Frame out{};
  assert(feed(badTag,corrupt,0,corrupt.length,out)==w::Read::Fatal);
  w::StreamDecoder stream(session,w::Direction::ToTaster,mac);
  assert(feed(stream,packet,0,packet.length,out)==w::Read::Ready);
  assert(feed(stream,packet,0,packet.length,out)==w::Read::Fatal);
  // Core can publish a new full state immediately after takeover.
  c.tasterAllowed=false; ++c.authorityGeneration;
  n.lifecycle=SessionLifecycle::Paused; c.lifecycle=Lifecycle::Paused;
  ++c.lifecycleRevision; n.startPhase=StartPresentationPhase::ResumeCountdown;
  assert(w::projectDisplay(sample,&prefs,3,c,888888,s));
  assert(!s.tasterAllowed && s.lifecycle==w::DisplayLifecycle::Paused &&
         s.phase==w::Phase::ResumeCountdown);
  // Finished state and committed result, not a locally calculated outcome.
  c.lifecycle=Lifecycle::Finished; n.lifecycle=SessionLifecycle::Finished;
  n.resultSealed=true; n.deadHeat=true; n.finishDisplayActive=true;
  assert(w::projectDisplay(sample,&prefs,3,c,999999,s));
  assert((s.flags&(w::ResultSealed|w::DeadHeat|w::FinishDisplay))==
         (w::ResultSealed|w::DeadHeat|w::FinishDisplay));
  // Reject inconsistent context/revision and expanded lanes, never truncate.
  const auto good=s;
  ++c.proposalRevision; assert(!w::projectDisplay(sample,&prefs,3,c,0,s)); --c.proposalRevision;
  c.lifecycle=Lifecycle::Racing; assert(!w::projectDisplay(sample,&prefs,3,c,0,s));
  c.lifecycle=Lifecycle::Finished;
  n.entryCount=3; assert(!w::projectDisplay(sample,&prefs,3,c,0,s)); n.entryCount=2;
  assert(s.coreTimeUs==good.coreTimeUs);
  n.entries[1].lane=2; assert(!w::projectDisplay(sample,&prefs,3,c,0,s)); n.entries[1].lane=1;
  std::memset(prefs.lane1Label,'X',sizeof(prefs.lane1Label));
  assert(!w::projectDisplay(sample,&prefs,3,c,0,s));
  // Missing preferences is explicit, with generic/factory fallback semantics.
  n.entryCount=0; n.lifecycle=SessionLifecycle::Ready; c.lifecycle=Lifecycle::Ready;
  n.sessionMode=SessionMode::None; n.startPhase=StartPresentationPhase::None;
  assert(w::projectDisplay(sample,nullptr,0,c,1000000,s));
  assert(!(s.flags&w::PreferencesPresent) && w::emptyLane(s.lanes[0]) && w::emptyLane(s.lanes[1]));
  // Browser Practice is represented, never re-labelled as LAPS.
  n.lifecycle=SessionLifecycle::Racing; c.lifecycle=Lifecycle::Racing;
  n.sessionMode=SessionMode::OpenPractice;
  assert(w::projectDisplay(sample,nullptr,0,c,1000001,s));
  assert(s.activeMode==w::DisplayMode::Practice && !s.tasterAllowed);
  c.tasterAllowed=true; assert(!w::projectDisplay(sample,nullptr,0,c,1000001,s));
  std::cout<<"S3c display snapshot projection, authenticated framing and malformed-input checks PASS\n";
}
