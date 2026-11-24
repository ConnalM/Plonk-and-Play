#include "pp/taster_link_wire.h"

#include <openssl/hmac.h>
#include <openssl/crypto.h>
#include <algorithm>
#include <cassert>
#include <iostream>

namespace w = pp::taster_link::wire;
using namespace pp::taster_link;

namespace {
class TestMac final : public w::Mac {
public:
  explicit TestMac(uint8_t marker) { key_.fill(marker); }
  bool sign(const uint8_t* bytes, size_t length, uint8_t* tag) const override {
    unsigned n=0;
    return HMAC(EVP_sha256(), key_.data(), static_cast<int>(key_.size()),
                bytes, length, tag, &n) != nullptr && n == w::TagSize;
  }
  bool verify(const uint8_t* bytes, size_t length, const uint8_t* tag) const override {
    std::array<uint8_t,w::TagSize> expected{};
    return sign(bytes,length,expected.data()) &&
           CRYPTO_memcmp(expected.data(),tag,expected.size()) == 0;
  }
private:
  // Deterministic test keys only; never production provisioning.
  std::array<uint8_t,32> key_{};
};
class BrokenMac final : public w::Mac {
public:
  bool sign(const uint8_t*,size_t,uint8_t*) const override { return false; }
  bool verify(const uint8_t*,size_t,const uint8_t*) const override { return false; }
};
w::Read feed(w::StreamDecoder& d,const w::Packet& p,size_t first,size_t last,w::Frame& f) {
  w::Read r=w::Read::NeedMore;
  for (size_t i=first;i<last;++i) {
    r=d.push(p.bytes[i],f);
    if (r!=w::Read::NeedMore) {
      if (r==w::Read::Ready) assert(i+1==last);
      break;
    }
  }
  return r;
}
w::Read feed(w::StreamDecoder& d,const w::Packet& p,w::Frame& f) {
  return feed(d,p,0,p.length,f);
}
void retag(w::Packet& p,const TestMac& mac) {
  assert(mac.sign(p.bytes.data(),p.length-w::TagSize,p.bytes.data()+p.length-w::TagSize));
}
}
int main() {
  const Session s{0x1020304050607080ULL,0x1122334455667788ULL};
  const TestMac toCore(0x31),toTaster(0x82),wrongKey(0x99);
  const BrokenMac broken;
  constexpr uint64_t now=1000000;
  Context ctx{s,5,6,0,7,Lifecycle::Ready,true};
  Command q{};
  q.session=s; q.id=1; q.authorityGeneration=ctx.authorityGeneration;
  q.lifecycleRevision=ctx.lifecycleRevision; q.raceId=ctx.raceId;
  q.proposalRevision=ctx.proposalRevision; q.expiresAtUs=now+200000;
  q.action=Action::Setup; q.mode=Mode::Timed; q.value=999;
  w::Packet p;
  assert(w::seal(w::command(q,1),w::Direction::ToCore,toCore,p));
  assert(p.length==w::HeaderSize+49+w::TagSize);
  const uint8_t prefix[]={'P','P','T','L',1,1,0,49,0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80};
  assert(std::equal(std::begin(prefix),std::end(prefix),p.bytes.begin()));
  // Every possible two-chunk split, including partial header/tag.
  for (size_t split=0;split<p.length;++split) {
    w::StreamDecoder d(s,w::Direction::ToCore,toCore);
    w::Frame f{};
    if (split) {
      assert(feed(d,p,0,split,f)==w::Read::NeedMore);
      assert(d.partial());
    }
    assert(feed(d,p,split,p.length,f)==w::Read::Ready);
    assert(!d.partial() && !d.failed());
    Command decoded{};
    assert(w::readCommand(f,decoded) && decoded==q);
  }

  // One TCP stream can carry multiple adjacent records; application command
  // ID remains independent from authenticated record sequence.
  w::Packet duplicate, pingPacket;
  assert(w::seal(w::command(q,2),w::Direction::ToCore,toCore,duplicate));
  assert(w::seal(w::ping(s,3,{42,now}),w::Direction::ToCore,toCore,pingPacket));
  w::StreamDecoder receiver(s,w::Direction::ToCore,toCore);
  w::Frame f{};
  CommandGate gate;
  assert(gate.open(s));
  assert(feed(receiver,p,f)==w::Read::Ready);
  Command received{}, dispatched{};
  assert(w::readCommand(f,received));
  assert(gate.submit(received,ctx,now)==Status::Queued);
  assert(gate.take(ctx,now,dispatched) && dispatched==q);
  assert(gate.complete(s,q.id,true));
  assert(feed(receiver,duplicate,f)==w::Read::Ready);
  assert(w::readCommand(f,received));
  assert(gate.submit(received,ctx,now)==Status::Accepted);
  assert(!gate.take(ctx,now,dispatched));
  assert(feed(receiver,pingPacket,f)==w::Read::Ready);
  w::Ping ping{};
  assert(w::readPing(f,ping) && ping.token==42 && ping.tasterSendUs==now);

  w::Packet statePacket,resultPacket,pongPacket;
  assert(w::seal(w::controlState({ctx,now},1),w::Direction::ToTaster,toTaster,statePacket));
  assert(w::seal(w::result(s,2,{q.id,gate.result(s,q.id)}),w::Direction::ToTaster,toTaster,resultPacket));
  assert(w::seal(w::pong(s,3,{42,now,now+40,now+50}),w::Direction::ToTaster,toTaster,pongPacket));
  w::StreamDecoder display(s,w::Direction::ToTaster,toTaster);
  assert(feed(display,statePacket,f)==w::Read::Ready);
  w::ControlState state{};
  assert(w::readControlState(f,state) && state.context.session==s &&
         state.context.authorityGeneration==5 && state.context.lifecycleRevision==6 &&
         state.context.proposalRevision==7 && state.context.lifecycle==Lifecycle::Ready &&
         state.context.tasterAllowed && state.coreTimeUs==now);
  assert(feed(display,resultPacket,f)==w::Read::Ready);
  w::Result result{};
  assert(w::readResult(f,result) && result.commandId==1 && result.status==Status::Accepted);
  assert(feed(display,pongPacket,f)==w::Read::Ready);
  w::Pong pong{};
  assert(w::readPong(f,pong) && pong.token==42 && pong.tasterSendUs==now &&
         pong.coreReceiveUs==now+40 && pong.coreSendUs==now+50);

  auto reject=[&](const w::Packet& invalid,Session session,w::Direction direction,const w::Mac& mac) {
    w::StreamDecoder d(session,direction,mac);
    w::Frame untouched{};
    untouched.sequence=123;
    assert(feed(d,invalid,untouched)==w::Read::Fatal);
    assert(d.failed() && untouched.sequence==123);
    assert(d.push(p.bytes[0],untouched)==w::Read::Fatal);
  };
  reject(p,s,w::Direction::ToCore,wrongKey);
  reject(p,s,w::Direction::ToCore,broken);
  reject(p,s,w::Direction::ToTaster,toCore); // reflection forbidden
  reject(p,{s.epoch,s.link+1},w::Direction::ToCore,toCore);
  reject(duplicate,s,w::Direction::ToCore,toCore); // sequence gap
  for (size_t i=0;i<p.length;++i) {
    auto corrupted=p; corrupted.bytes[i]^=1;
    reject(corrupted,s,w::Direction::ToCore,toCore);
  }
  // Even authenticated but noncanonical frames are invalid.
  auto bad=p; bad.bytes[4]=2; retag(bad,toCore); reject(bad,s,w::Direction::ToCore,toCore);
  bad=p; bad.bytes[5]=255; retag(bad,toCore); reject(bad,s,w::Direction::ToCore,toCore);
  bad=p; bad.bytes[6]=255; retag(bad,toCore); reject(bad,s,w::Direction::ToCore,toCore);
  bad=p; bad.bytes[w::HeaderSize+44]=255; retag(bad,toCore); reject(bad,s,w::Direction::ToCore,toCore);
  bad=p; bad.bytes[w::HeaderSize+45]=255; retag(bad,toCore); reject(bad,s,w::Direction::ToCore,toCore);
  bad=p; bad.bytes[w::HeaderSize+48]=2; retag(bad,toCore); reject(bad,s,w::Direction::ToCore,toCore);
  bad=p; std::fill_n(bad.bytes.begin()+w::HeaderSize,8,0); retag(bad,toCore);
  reject(bad,s,w::Direction::ToCore,toCore);

  // An actual replay of the same authenticated sequence is fatal. A fresh
  // authenticated link must use a fresh decoder and cannot accept old frames.
  w::StreamDecoder replay(s,w::Direction::ToCore,toCore);
  assert(feed(replay,p,f)==w::Read::Ready);
  assert(feed(replay,p,f)==w::Read::Fatal);
  w::StreamDecoder truncated(s,w::Direction::ToCore,toCore);
  assert(feed(truncated,p,0,p.length-1,f)==w::Read::NeedMore && truncated.partial());
  w::StreamDecoder fresh({s.epoch,s.link+1},w::Direction::ToCore,toCore);
  assert(feed(fresh,p,f)==w::Read::Fatal);

  w::Packet unused;
  assert(!w::seal(w::command(q,1),w::Direction::ToCore,broken,unused) && !unused.length);
  assert(!w::seal(w::command(q,1),w::Direction::ToTaster,toCore,unused) && !unused.length);
  assert(!w::seal(w::command(q,0),w::Direction::ToCore,toCore,unused));
  assert(!w::seal(w::result(s,1,{0,Status::Accepted}),w::Direction::ToTaster,toTaster,unused));
  assert(!w::seal(w::pong(s,1,{42,now,now+10,now}),w::Direction::ToTaster,toTaster,unused));
  assert(!w::seal(w::ping(s,1,{0,now}),w::Direction::ToCore,toCore,unused));
  std::cout<<"S3b authenticated wire framing and command exchange PASS\n";
}
