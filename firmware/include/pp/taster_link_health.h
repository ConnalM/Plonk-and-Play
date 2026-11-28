#pragma once
#include "taster_link_wire.h"
#include <cstdint>

namespace pp::taster_link::health {

// Provisional host engineering parameters, NOT sporting tolerances or a
// measured wireless acceptance claim. Relative clock rate bound includes
// both oscillators. If it cannot be demonstrated on the hardware, revise it.
struct Policy {
  static constexpr uint64_t StartupUs=2000000, SnapshotAgeUs=500000,
    PingTimeoutUs=250000, MappingAgeUs=5000000, PendingUs=2000000,
    RelativeDriftPpm=200;
};
struct Bounds { uint64_t lower=0,upper=0; };
inline bool sum(uint64_t a,uint64_t b,uint64_t& out) {
  if(b>UINT64_MAX-a) return false;
  out=a+b;return true;
}
// Inputs are bounded by MappingAgeUs + PingTimeoutUs, so the multiplication
// below cannot overflow. Round outward even when the interval is short.
inline uint64_t drift(uint64_t us) {
  return (us*Policy::RelativeDriftPpm+999999)/1000000;
}
class Clock {
public:
  void clear() { valid_=false; }
  bool update(uint64_t t0,uint64_t t3,uint64_t c1,uint64_t c2) {
    clear();
    if(t3<t0 || t3-t0>=Policy::PingTimeoutUs || c2<c1) return false;
    const auto rtt=t3-t0;
    if(c2-c1>rtt+drift(rtt)) return false;
    t0_=t0;t3_=t3;c1_=c1;c2_=c2;valid_=true;
    Bounds b;
    if(!at(t3,b)) {clear();return false;}
    return true;
  }
  bool at(uint64_t now,Bounds& out) const {
    if(!valid_ || now<t3_ || now-t3_>=Policy::MappingAgeUs) return false;
    const auto sinceReceive=now-t3_,sinceSend=now-t0_;
    const auto lowerDrift=drift(sinceReceive);
    // At a zero or sub-ppm interval drift rounds up; saturate at zero
    // rather than subtracting an unsigned negative duration.
    const auto safeLower=sinceReceive>=lowerDrift?sinceReceive-lowerDrift:0;
    Bounds b;
    if(!sum(c2_,safeLower,b.lower) ||
       !sum(c1_,sinceSend+drift(sinceSend),b.upper) || b.lower>b.upper) return false;
    out=b;return true;
  }
private:
  bool valid_=false;
  uint64_t t0_=0,t3_=0,c1_=0,c2_=0;
};

enum class State { Offline, Synchronising, Fresh, Stale };
struct View {
  State state=State::Offline;
  bool hasSnapshot=false,mayControl=false,uncertainOutcome=false;
  bool readyForNewTouch=false,gestureActive=false,awaitingPostResultSnapshot=false;
  uint64_t interactionRevision=0;
  wire::DisplaySnapshot snapshot{};
  Bounds coreNow{};
};

inline bool sameGestureContext(const wire::DisplaySnapshot& a,const wire::DisplaySnapshot& b) {
  return a.session==b.session && a.authorityGeneration==b.authorityGeneration &&
    a.lifecycleRevision==b.lifecycleRevision && a.raceId==b.raceId &&
    a.proposalRevision==b.proposalRevision && a.lifecycle==b.lifecycle &&
    a.tasterAllowed==b.tasterAllowed && a.readiness==b.readiness &&
    a.activeMode==b.activeMode && a.proposalMode==b.proposalMode;
}

// One client-owner loop. All incoming frames must already have passed the
// authenticated session/direction/sequence decoder. The caller timestamps
// sends/receives and services tick() even without network or touch events.
// No access to Core race control, no automatic replay, no UI/transport calls.
class Client {
public:
  Client()=default;
  Client(const Client&)=delete;
  Client& operator=(const Client&)=delete;
  bool open(Session fresh,uint64_t now) {
    if(connected_ || !fresh.valid() || fresh==session_ || !observeTime(now)) return false;
    if(have_ && fresh.epoch!=last_.session.epoch) {have_=false;last_={};}
    session_=fresh;connected_=true;opened_=now;fresh_=false;clock_.clear();
    mapped_=false;pingPending_=false;token_=0;firstSampleAfter_=0;lastIssued_=0;
    postResult_=false;postResultAfter_=0;haveResult_=false;result_={};cancelTouch();
    return true;
  }
  void disconnect() { retire(); }
  bool connected() const {return connected_;}
  bool tick(uint64_t now) {
    if(!observeTime(now)) {retire();return false;}
    if(!connected_) return false;
    Bounds b;
    if(!fresh_ && now-opened_>=Policy::StartupUs) {retire();return false;}
    if(pingPending_ && now-pingSent_>=Policy::PingTimeoutUs) {retire();return false;}
    if(mapped_ && !clock_.at(now,b)) {retire();return false;}
    if(fresh_ && (!mapped_ || !sampleAge(last_,b))) {retire();return false;}
    if(pendingId_ && now-pendingSince_>=Policy::PendingUs) {retire();return false;}
    return true;
  }
  bool startPing(uint64_t now,wire::Ping& out) {
    if(!tick(now) || pingPending_) return false;
    if(token_==UINT64_MAX) {retire();return false;}
    ++token_;pingSent_=now;pingPending_=true;
    out={token_,now};return true;
  }
  bool acceptPong(Session from,const wire::Pong& pong,uint64_t now) {
    if(!tick(now)) return false;
    Clock candidate;
    if(!(from==session_) || !pingPending_ || pong.token!=token_ ||
       pong.tasterSendUs!=pingSent_ ||
       !candidate.update(pingSent_,now,pong.coreReceiveUs,pong.coreSendUs)) {
      retire();return false;
    }
    // Two valid observations of the same clock must admit a common current
    // time under the declared drift bound. A discontinuity is not a new
    // offset to silently trust within the same authenticated session.
    Bounds oldBounds,newBounds;
    if(mapped_ && (!clock_.at(now,oldBounds) || !candidate.at(now,newBounds) ||
       oldBounds.lower>newBounds.upper || newBounds.lower>oldBounds.upper)) {
      retire();return false;
    }
    clock_=candidate;
    pingPending_=false;
    if(!mapped_) firstSampleAfter_=pong.coreSendUs;
    mapped_=true;
    return tick(now);
  }
  bool acceptSnapshot(const wire::DisplaySnapshot& s,uint64_t now) {
    if(!tick(now)) return false;
    if(!(s.session==session_) || !wire::validDisplay(s)) {retire();return false;}
    // Before the initial matched pong no timestamp is trustworthy enough
    // for a current display. Ignore those samples, never enable controls.
    if(!mapped_) return false;
    Bounds b;
    if(!clock_.at(now,b) || !sampleAge(s,b) || s.coreTimeUs<firstSampleAfter_ ||
       (fresh_ && s.coreTimeUs<last_.coreTimeUs)) {retire();return false;}
    if(!fresh_ || !sameGestureContext(last_,s)) cancelTouch();
    if(pendingId_ && (!s.tasterAllowed ||
       s.authorityGeneration!=last_.authorityGeneration)) forgetPending();
    // A newer arrival is not necessarily a newer authoritative sample.
    // Keep the barrier through equal or queued pre-result snapshots.
    if(postResult_ && s.coreTimeUs>postResultAfter_) {
      postResult_=false;postResultAfter_=0;cancelTouch();
    }
    last_=s;have_=true;fresh_=true;
    return true;
  }
  View view(uint64_t now) {
    tick(now);
    View v{};
    v.state=fresh_?State::Fresh:(connected_?State::Synchronising:(have_?State::Stale:State::Offline));
    v.hasSnapshot=have_;v.uncertainOutcome=uncertain_;
    v.interactionRevision=interactionRevision_;
    if(have_) v.snapshot=last_;
    if(fresh_) {
      if(!clock_.at(now,v.coreNow)) {retire();return view(now);}
      v.awaitingPostResultSnapshot=postResult_;
      v.mayControl=mayControl();
      // The renderer must not mistake cached Core permission for local
      // permission while a request or post-result barrier is outstanding.
      v.snapshot.tasterAllowed=v.mayControl;
      v.readyForNewTouch=v.mayControl && neutral_ && !touch_;
      v.gestureActive=v.mayControl && touch_;
      // Display only sampled values. Never synthesize GO or interpolate
      // clocks. Suppress a sampled GO once its validity may have elapsed.
      if(v.snapshot.phase!=wire::Phase::Go || !v.snapshot.phaseUntilUs ||
         v.coreNow.upper>=v.snapshot.phaseUntilUs) {
        if(v.snapshot.phase==wire::Phase::Go) v.snapshot.phase=wire::Phase::None;
        v.snapshot.flags&=static_cast<uint16_t>(~wire::Green);
      }
    } else {
      // The renderer MUST label this retained data stale/synchronising.
      // No old light, control authority, or running timer extrapolation.
      v.snapshot.tasterAllowed=false;v.snapshot.redLit=0;v.snapshot.phase=wire::Phase::None;
      v.snapshot.flags&=static_cast<uint16_t>(~wire::Green);
    }
    return v;
  }
  // Renderer must report the actual touch-contact level, not fabricate a
  // release when cancelling a hold. After invalidation a new neutral sample
  // and a new physical down/up are required. The renderer still enforces
  // the existing 700 ms pause hold and button-specific confirmation rules.
  void observeReleased(uint64_t now) {
    if(tick(now) && mayControl() && !touch_) neutral_=true;
  }
  bool beginTouch(uint64_t now) {
    if(!tick(now) || !mayControl() || !neutral_ || touch_) return false;
    neutral_=false;touch_=true;return true;
  }
  void cancelTouch() {
    touch_=false;neutral_=false;
    // A saturated revision cannot signal future invalidations. Permanently
    // disallow touch initiation on this client rather than silently wrap.
    if(interactionRevision_!=UINT64_MAX) ++interactionRevision_;
  }
  // Called only for a valid release/confirmed action, on the same owner
  // loop as transport enqueue. Returns a context, not an execution result.
  // ID must be fresh per session. No retry of the pending command.
  bool releaseForCommand(uint64_t now,uint64_t id,Context& out) {
    if(!tick(now) || !mayControl() || !touch_ ||
       !id || id<=lastIssued_) {cancelTouch();return false;}
    cancelTouch();lastIssued_=id;pendingId_=id;pendingSince_=now;
    out=wire::commandContext(last_);return true;
  }
  // A failed or uncertain transport enqueue closes the link. Never claim
  // the command did not execute merely because a reply is absent.
  void sendUncertain() {retire();}
  bool acceptResult(Session from,const wire::Result& r,uint64_t now) {
    if(!tick(now) || !(from==session_) || !pendingId_ || r.commandId!=pendingId_) return false;
    if(r.status==Status::Queued || r.status==Status::InFlight) return true;
    Bounds b;
    if(!clock_.at(now,b)) {retire();return false;}
    // Every terminal result closes input until a coherent Core sample
    // is provably later than receipt of this result. Use the conservative
    // upper bound, not the last frame timestamp or arrival order: a queued
    // snapshot may be newer than the command's sample yet predate execution.
    // The latest sample cannot exceed this bound (tick checked its age).
    postResult_=true;postResultAfter_=b.upper;
    if(r.status==Status::Unknown || r.status==Status::WrongSession) {
      forgetPending();
    } else {
      result_=r;haveResult_=true;pendingId_=0;
    }
    // No mutation of lifecycle, lap count, setup or timing from results.
    cancelTouch();return true;
  }
  bool pending() const {return pendingId_!=0;}
  // A terminal authenticated result is consumed exactly once by the
  // presentation adapter.  Unknown/WrongSession outcomes deliberately do
  // not become a rejection: the command's real-world outcome is uncertain.
  bool takeResult(uint64_t id, Status& status) {
    if(!haveResult_ || !id || result_.commandId!=id) return false;
    status=result_.status;haveResult_=false;result_={};return true;
  }
  // Adapter acknowledges the indication only after surfacing it; not a race
  // acknowledgement and not a command retransmission.
  void acknowledgeUncertainty() {uncertain_=false;}
private:
  bool mayControl() const {
    return connected_ && fresh_ && last_.tasterAllowed && !pendingId_ &&
      !postResult_ && interactionRevision_!=UINT64_MAX;
  }
  bool observeTime(uint64_t now) {
    if(haveTime_ && now<lastNow_) return false;
    haveTime_=true;lastNow_=now;return true;
  }
  static bool sampleAge(const wire::DisplaySnapshot& s,const Bounds& b) {
    return s.coreTimeUs<=b.upper && b.upper-s.coreTimeUs<Policy::SnapshotAgeUs;
  }
  void forgetPending() {if(pendingId_) uncertain_=true;pendingId_=0;}
  void retire() {
    forgetPending();cancelTouch();connected_=false;fresh_=false;
    mapped_=false;clock_.clear();pingPending_=false;lastIssued_=0;
    postResult_=false;postResultAfter_=0;haveResult_=false;result_={};
  }
  Session session_{};
  Clock clock_{};
  wire::DisplaySnapshot last_{};
  uint64_t opened_=0,lastNow_=0,token_=0,pingSent_=0,firstSampleAfter_=0,
    pendingId_=0,pendingSince_=0,lastIssued_=0,interactionRevision_=0,postResultAfter_=0;
  wire::Result result_{};
  bool haveTime_=false,connected_=false,mapped_=false,pingPending_=false,
    have_=false,fresh_=false,neutral_=false,touch_=false,uncertain_=false,postResult_=false,
    haveResult_=false;
};

} // namespace pp::taster_link::health
