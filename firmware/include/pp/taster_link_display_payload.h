#pragma once

#include "taster_link_gate.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace pp::taster_link::wire {

// Fixed two-lane display snapshot, independent of PP_MAX_ENTRIES and native
// object layout. Not the future configurable auxiliary display schema.
enum class DisplayLifecycle : uint8_t { Ready=0, Starting=1, Racing=2, Paused=3, Restarting=4, Finished=5, Faulted=6 };
enum class DisplayMode : uint8_t { None=0, Laps=1, Practice=2, Timed=3 };
enum class Readiness : uint8_t { Ready=0, NotStartable=1, InvalidSetup=2, CapabilityUnavailable=3 };
enum class Phase : uint8_t { None=0, Countdown=1, ResumeCountdown=2, Go=3 };
enum class FinishPolicy : uint8_t { Immediate=0, CurrentLap=1, FullDistance=2 };
enum class Restart : uint8_t { None=0, Honour=1, Grid=2 };
enum class Gap : uint8_t { None=0, Laps=1, Time=2 };
enum class Graphic : uint8_t { GreenMgb=0, RedEtype=1 };

enum DisplayFlags : uint16_t {
  DurationExpired=1u, Simulated=2u, Green=4u, ResultSealed=8u,
  ResultValid=16u, DeadHeat=32u, IntegrityFault=64u, FinishDisplay=128u,
  PersistencePending=256u, PersistenceFault=512u, PreferencesPresent=1024u
};
constexpr uint16_t DisplayFlagMask=2047u;
enum LaneFlags : uint8_t { HasLap=1u, Completed=2u, HasOvertime=4u, WaitingOrigin=8u };
constexpr uint8_t LaneFlagMask=15u;
constexpr size_t LabelSize=25;
struct DisplayLane {
  uint8_t lane=0, flags=0;
  Gap gap=Gap::None;
  uint32_t laps=0, classified=0, penalty=0, position=0, lapsBehind=0;
  uint64_t lastLapUs=0, bestLapUs=0, finishUs=0, overtimeUs=0, gapUs=0;
};
struct DisplaySnapshot {
  Session session{};
  uint64_t authorityGeneration=0, lifecycleRevision=0, raceId=0, coreTimeUs=0;
  uint32_t proposalRevision=0;
  DisplayLifecycle lifecycle=DisplayLifecycle::Faulted;
  bool tasterAllowed=false;
  DisplayMode activeMode=DisplayMode::None, proposalMode=DisplayMode::None;
  Readiness readiness=Readiness::NotStartable;
  FinishPolicy activeFinish=FinishPolicy::Immediate, proposalFinish=FinishPolicy::Immediate;
  Restart restart=Restart::None;
  Phase phase=Phase::None;
  uint8_t redCount=0, redLit=0, startSignal=0, entryCount=0, proposedLanes=0;
  uint32_t proposalLaps=0, activeLaps=0;
  uint16_t proposalMinutes=0, activeMinutes=0, flags=0;
  uint64_t elapsedUs=0, remainingUs=0, overtimeUs=0, goUs=0, phaseUntilUs=0,
           pauseUs=0, restartUs=0, expiryUs=0, finishUntilUs=0;
  // Publication revision supplied by the future Core owner, not a second
  // persistence generation. Full committed labels are included every time.
  uint64_t preferencesRevision=0;
  std::array<Graphic,2> graphics{Graphic::GreenMgb,Graphic::RedEtype};
  std::array<std::array<char,LabelSize>,2> labels{};
  std::array<DisplayLane,2> lanes{};
};
inline Lifecycle gateLifecycle(DisplayLifecycle x) {
  switch(x) {
    case DisplayLifecycle::Ready: return Lifecycle::Ready;
    case DisplayLifecycle::Starting: return Lifecycle::Starting;
    case DisplayLifecycle::Racing: return Lifecycle::Racing;
    case DisplayLifecycle::Paused: return Lifecycle::Paused;
    case DisplayLifecycle::Finished: return Lifecycle::Finished;
    default: return Lifecycle::Other; // never permits a fault/restart as READY
  }
}
inline Context commandContext(const DisplaySnapshot& s) {
  return {s.session,s.authorityGeneration,s.lifecycleRevision,s.raceId,
          s.proposalRevision,gateLifecycle(s.lifecycle),s.tasterAllowed};
}

// Match the existing committed Taster label contract (24 bytes, <=16
// characters, excluded control/quote/backslash bytes and continuation shape).
// Require a NUL and canonical zero padding. Full Unicode/glyph validation is
// deliberately not a new policy in this transport increment.
inline bool validDisplayLabel(const std::array<char,LabelSize>& label) {
  size_t i=0, chars=0;
  while(i<LabelSize && label[i]) {
    const auto first=static_cast<uint8_t>(label[i]);
    if(i>=24 || chars>=16 || first<0x20 || first==0x7f || first==34 || first==92) return false;
    size_t width=1;
    if(first>=0x80) {
      if((first&0xe0)==0xc0) width=2;
      else if((first&0xf0)==0xe0) width=3;
      else if((first&0xf8)==0xf0) width=4;
      else return false;
      if(i+width>24) return false;
      for(size_t j=1;j<width;++j)
        if((static_cast<uint8_t>(label[i+j])&0xc0)!=0x80) return false;
    }
    i+=width; ++chars;
  }
  if(i==LabelSize) return false;
  for(;i<LabelSize;++i) if(label[i]) return false;
  return true;
}

template<typename E> inline bool enumAtMost(E x,E last) {
  return static_cast<uint8_t>(x)<=static_cast<uint8_t>(last);
}
inline bool emptyLane(const DisplayLane& l) {
  return !l.lane && !l.flags && l.gap==Gap::None && !l.laps && !l.classified &&
    !l.penalty && !l.position && !l.lapsBehind && !l.lastLapUs && !l.bestLapUs &&
    !l.finishUs && !l.overtimeUs && !l.gapUs;
}
inline bool validDisplay(const DisplaySnapshot& s) {
  if(!s.session.valid() || !enumAtMost(s.lifecycle,DisplayLifecycle::Faulted) ||
     !enumAtMost(s.activeMode,DisplayMode::Timed) || !enumAtMost(s.proposalMode,DisplayMode::Timed) ||
     !enumAtMost(s.readiness,Readiness::CapabilityUnavailable) ||
     !enumAtMost(s.activeFinish,FinishPolicy::FullDistance) ||
     !enumAtMost(s.proposalFinish,FinishPolicy::FullDistance) ||
     !enumAtMost(s.restart,Restart::Grid) || !enumAtMost(s.phase,Phase::Go) ||
     s.entryCount>2 || s.proposedLanes>2 || s.redCount>5 ||
     s.redLit>s.redCount || s.startSignal>1 || (s.flags&~DisplayFlagMask)) return false;
  if((s.lifecycle==DisplayLifecycle::Faulted || s.lifecycle==DisplayLifecycle::Restarting ||
      s.activeMode==DisplayMode::Practice || s.proposalMode==DisplayMode::Practice) &&
     s.tasterAllowed) return false;
  if(!(s.flags&PreferencesPresent) &&
     (s.preferencesRevision || s.labels[0][0] || s.labels[1][0] ||
      s.graphics[0]!=Graphic::GreenMgb || s.graphics[1]!=Graphic::RedEtype)) return false;
  for(size_t i=0;i<2;++i) {
    if(!enumAtMost(s.graphics[i],Graphic::RedEtype) || !validDisplayLabel(s.labels[i])) return false;
    const auto& l=s.lanes[i];
    if(i>=s.entryCount) { if(!emptyLane(l)) return false; continue; }
    if(l.lane<1 || l.lane>2 || (i && l.lane==s.lanes[0].lane) ||
       (l.flags&~LaneFlagMask) || !enumAtMost(l.gap,Gap::Time)) return false;
    // Do not infer classifications, winner, or a gap from numeric fields.
    // The explicit gap/presence kinds govern rendering.
  }
  return true;
}

// Exact fixed-size payload: 196-byte common state + 2*63-byte lane slots.
// Unused lane slots are all zero. Encoder and decoder share explicit widths.
constexpr size_t DisplayPayloadSize=322;
inline void displayPut(uint8_t*& p,uint64_t v,unsigned n) {
  for(unsigned i=n;i;--i) *p++=static_cast<uint8_t>(v>>(8u*(i-1u)));
}
inline uint64_t displayGet(const uint8_t*& p,unsigned n) {
  uint64_t v=0; for(unsigned i=0;i<n;++i) v=(v<<8u)|*p++; return v;
}
inline bool encodeDisplay(const DisplaySnapshot& s,uint8_t* bytes,size_t length) {
  if(!bytes || length!=DisplayPayloadSize || !validDisplay(s)) return false;
  auto* p=bytes;
  auto u=[&](uint64_t v,unsigned n){displayPut(p,v,n);};
  auto e=[&](auto v){u(static_cast<uint8_t>(v),1);};
  u(s.authorityGeneration,8); u(s.lifecycleRevision,8); u(s.raceId,8); u(s.proposalRevision,4);
  u(s.coreTimeUs,8); e(s.lifecycle); u(s.tasterAllowed?1u:0u,1);
  e(s.activeMode); e(s.proposalMode); e(s.readiness); e(s.activeFinish); e(s.proposalFinish);
  e(s.restart); e(s.phase); u(s.redCount,1); u(s.redLit,1); u(s.startSignal,1);
  u(s.entryCount,1); u(s.proposedLanes,1);
  u(s.proposalLaps,4); u(s.activeLaps,4); u(s.proposalMinutes,2); u(s.activeMinutes,2); u(s.flags,2);
  u(s.elapsedUs,8); u(s.remainingUs,8); u(s.overtimeUs,8); u(s.goUs,8); u(s.phaseUntilUs,8);
  u(s.pauseUs,8); u(s.restartUs,8); u(s.expiryUs,8); u(s.finishUntilUs,8);
  u(s.preferencesRevision,8);
  for(size_t i=0;i<2;++i) { e(s.graphics[i]); std::memcpy(p,s.labels[i].data(),LabelSize); p+=LabelSize; }
  for(const auto& l:s.lanes) {
    u(l.lane,1); u(l.flags,1); e(l.gap); u(l.laps,4); u(l.classified,4); u(l.penalty,4);
    u(l.position,4); u(l.lapsBehind,4); u(l.lastLapUs,8); u(l.bestLapUs,8);
    u(l.finishUs,8); u(l.overtimeUs,8); u(l.gapUs,8);
  }
  return static_cast<size_t>(p-bytes)==length;
}
inline bool decodeDisplay(Session session,const uint8_t* bytes,size_t length,DisplaySnapshot& out) {
  if(!bytes || length!=DisplayPayloadSize) return false;
  DisplaySnapshot s{}; s.session=session;
  const auto* p=bytes;
  auto u=[&](unsigned n){return displayGet(p,n);};
  auto e=[&](auto& v){v=static_cast<std::decay_t<decltype(v)>>(u(1));};
  s.authorityGeneration=u(8); s.lifecycleRevision=u(8); s.raceId=u(8); s.proposalRevision=static_cast<uint32_t>(u(4));
  s.coreTimeUs=u(8); e(s.lifecycle); const auto allowed=u(1); if(allowed>1) return false; s.tasterAllowed=allowed!=0;
  e(s.activeMode); e(s.proposalMode); e(s.readiness); e(s.activeFinish); e(s.proposalFinish);
  e(s.restart); e(s.phase); s.redCount=u(1); s.redLit=u(1); s.startSignal=u(1);
  s.entryCount=u(1); s.proposedLanes=u(1);
  s.proposalLaps=u(4); s.activeLaps=u(4); s.proposalMinutes=u(2); s.activeMinutes=u(2); s.flags=u(2);
  s.elapsedUs=u(8); s.remainingUs=u(8); s.overtimeUs=u(8); s.goUs=u(8); s.phaseUntilUs=u(8);
  s.pauseUs=u(8); s.restartUs=u(8); s.expiryUs=u(8); s.finishUntilUs=u(8);
  s.preferencesRevision=u(8);
  for(size_t i=0;i<2;++i) { e(s.graphics[i]); std::memcpy(s.labels[i].data(),p,LabelSize); p+=LabelSize; }
  for(auto& l:s.lanes) {
    l.lane=u(1); l.flags=u(1); e(l.gap); l.laps=u(4); l.classified=u(4); l.penalty=u(4);
    l.position=u(4); l.lapsBehind=u(4); l.lastLapUs=u(8); l.bestLapUs=u(8);
    l.finishUs=u(8); l.overtimeUs=u(8); l.gapUs=u(8);
  }
  if(static_cast<size_t>(p-bytes)!=length || !validDisplay(s)) return false;
  out=s; return true;
}

} // namespace pp::taster_link::wire
