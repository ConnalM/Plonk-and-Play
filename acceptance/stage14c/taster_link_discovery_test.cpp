#include "pp/taster_link_discovery.h"
#include <cassert>
#include <iostream>
namespace d=pp::taster_link::discovery;
namespace h=pp::taster_link::handshake;
class Store final:public d::HintStore {
public:
  d::HintRecord record{};
  bool present=false,readError=false,writeError=false,badReadback=false;
  unsigned writes=0;
  d::Read read(d::HintRecord& out) override{
    if(readError)return d::Read::Error;
    if(!present)return d::Read::Missing;
    out=record;if(badReadback && writes)out[9]^=1;
    return d::Read::Found;
  }
  bool replace(const d::HintRecord& in) override{
    ++writes;if(writeError)return false;
    record=in;present=true;return true;
  }
};
static d::Candidate candidate(unsigned id,int16_t rssi=-50,uint8_t channel=6){
  return {{0x02,0x12,0x34,0x56,0x78,static_cast<uint8_t>(id)},channel,rssi};
}
int main(){
  h::PairId pair{},other{};pair.fill(0x12);other.fill(0x34);
  const auto a=candidate(1,-90),b=candidate(2,-15);
  using S=d::State;using C=d::Credential;using I=d::Intent;
  Store store;d::Selector s(store);
  assert(s.load(C::Empty)==S::Idle);
  assert(s.load(C::Paired,pair)==S::Idle); // immutable load per instance
  auto scan=s.beginScan(100);assert(scan);
  assert(s.finishScan(scan,true,101)==S::NoneFound);
  scan=s.beginScan(102);
  assert(s.observe(scan,a) && s.observe(scan,candidate(1,-30)));
  assert(s.count()==1 && s.candidate(0)->rssi==-30);
  assert(s.finishScan(scan,true,103)==S::Connecting);
  auto attempt=s.attempt();
  assert(attempt.valid() && attempt.candidate.bssid==a.bssid && attempt.intent==I::FirstUse);
  assert(!store.writes && !s.remembered().valid() && !s.beginScan(104));
  assert(!s.authenticated(attempt.token+1,a.bssid,pair) && !store.writes);
  assert(s.authenticated(attempt.token,a.bssid,pair));
  assert(s.state()==S::Connected && s.remembered().pair==pair && store.writes==1);
  assert(!s.beginScan(105));
  s.disconnected();scan=s.beginScan(106);
  assert(s.observe(scan,b) && s.observe(scan,a));
  assert(s.finishScan(scan,true,107)==S::Connecting);
  auto reconnect=s.attempt();
  assert(reconnect.token!=attempt.token && reconnect.intent==I::Remembered &&
    reconnect.candidate.bssid==a.bssid && reconnect.expectedPair==pair);
  assert(!s.failed(attempt.token,true));
  assert(s.authenticated(reconnect.token,a.bssid,pair) && store.writes==1);
  s.disconnected();scan=s.beginScan(108);assert(s.observe(scan,b));
  assert(s.finishScan(scan,true,109)==S::WaitingRemembered && !s.attempt().valid());
  scan=s.beginScan(110);assert(s.observe(scan,candidate(1,-70,11)));
  assert(s.finishScan(scan,true,111)==S::Connecting && s.attempt().candidate.channel==11);
  assert(s.failed(s.attempt().token,true) && s.state()==S::Retry);
  // Remembered association survives reboot only with matching vault pair.
  d::Selector reboot(store);assert(reboot.load(C::Paired,pair)==S::Idle);
  scan=reboot.beginScan(1);assert(reboot.observe(scan,a));
  assert(reboot.finishScan(scan,true,2)==S::Connecting);
  assert(!reboot.authenticated(reboot.attempt().token,a.bssid,other));
  assert(reboot.state()==S::IdentityMismatch && store.writes==1 && !reboot.beginScan(3));
  d::Selector wrong(store),empty(store);
  assert(wrong.load(C::Paired,other)==S::IdentityMismatch);
  assert(empty.load(C::Empty)==S::IdentityMismatch);
  Store missing;d::Selector lost(missing);
  assert(lost.load(C::Paired,pair)==S::IdentityMismatch);
  Store bad;bad.present=true;bad.record=store.record;bad.record[8]^=1;
  d::Selector corrupt(bad);assert(corrupt.load(C::Paired,pair)==S::Fault);
  Store unreadable;unreadable.readError=true;
  d::Selector error(unreadable);assert(error.load(C::Empty)==S::Fault);
  Store noAuth;d::Selector vaultFault(noAuth);assert(vaultFault.load(C::Fault)==S::Fault);
  // Multiple Cores: no strongest-signal auto-selection, stable ordered list.
  Store choices;d::Selector multi(choices);assert(multi.load(C::Empty)==S::Idle);
  scan=multi.beginScan(20);assert(multi.observe(scan,b) && multi.observe(scan,a));
  assert(multi.finishScan(scan,true,21)==S::Choose);
  assert(!multi.attempt().valid() && multi.candidate(0)->bssid==a.bssid);
  assert(!multi.choose(scan-1,b.bssid,22));
  assert(!multi.choose(scan,candidate(3).bssid,23));
  assert(multi.choose(scan,a.bssid,24) && multi.attempt().candidate.bssid==a.bssid);
  assert(!multi.authenticated(multi.attempt().token,b.bssid,pair) && !choices.writes);
  assert(multi.state()==S::IdentityMismatch);
  Store retry;d::Selector t(retry);assert(t.load(C::Empty)==S::Idle);
  scan=t.beginScan(10);assert(t.observe(scan,a) && t.observe(scan,b));
  assert(t.finishScan(scan,true,11)==S::Choose);
  assert(!t.choose(scan,b.bssid,11+d::Selector::ChoiceUs) && t.state()==S::Retry);
  auto now=12+d::Selector::ChoiceUs;
  auto next=t.beginScan(now);assert(next!=scan);
  assert(!t.observe(scan,a));
  assert(t.finishScan(scan,true,now)==S::Scanning);
  assert(t.observe(next,b) && t.finishScan(next,true,++now)==S::Connecting);
  assert(t.failed(t.attempt().token,false) && t.state()==S::IdentityMismatch);
  // Incomplete/oversized/conflicting scan cannot appear to have one Core.
  Store overflow;d::Selector o(overflow);assert(o.load(C::Empty)==S::Idle);
  scan=o.beginScan(1);assert(o.observe(scan,a));
  assert(o.finishScan(scan,false,2)==S::Incomplete && !o.attempt().valid() && !o.count());
  scan=o.beginScan(3);assert(o.observe(scan,a) && !o.observe(scan,candidate(1,-70,1)));
  assert(o.finishScan(scan,true,4)==S::Incomplete);
  scan=o.beginScan(5);assert(!o.observe(scan,d::Candidate{}));
  assert(o.finishScan(scan,true,6)==S::Incomplete);
  scan=o.beginScan(7);
  for(unsigned i=1;i<=d::Selector::MaxCandidates;++i)assert(o.observe(scan,candidate(i)));
  assert(!o.observe(scan,candidate(9)) && o.finishScan(scan,true,8)==S::Incomplete);
  scan=o.beginScan(9);assert(o.observe(scan,a));
  assert(o.finishScan(scan,true,10)==S::Connecting);
  assert(o.failed(o.attempt().token,true) && o.state()==S::Retry);
  o.tick(9);assert(o.state()==S::Fault && !o.beginScan(11));
  // Storage failure after first-use auth cannot enable controls or retry
  // another candidate, even when vault installation already succeeded.
  for(bool badReadback:{false,true}){
    Store f;f.writeError=!badReadback;f.badReadback=badReadback;
    d::Selector v(f);assert(v.load(C::Empty)==S::Idle);
    scan=v.beginScan(1);assert(v.observe(scan,a));
    assert(v.finishScan(scan,true,2)==S::Connecting);
    assert(!v.authenticated(v.attempt().token,a.bssid,pair));
    assert(v.state()==S::Fault && !v.beginScan(3));
  }
  std::cout<<"S3g2d automatic Core selection and remembered identity policy PASS\n";
}
