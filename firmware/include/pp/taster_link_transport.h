#pragma once
#include "taster_link_handshake.h"
#include "taster_link_health.h"
#include <optional>

namespace pp::taster_link::transport {

// A connected, exclusive, NONBLOCKING byte stream. Positive = progress,
// zero = would block, negative = EOF/error. Never wait, allocate an
// unbounded queue, reconnect, or call back into the owner. close is idempotent.
class Stream {
public:
  virtual ~Stream()=default;
  virtual int read(uint8_t*,size_t)=0;
  virtual int write(const uint8_t*,size_t)=0;
  virtual void close()=0;
};
class ClockSource {
public:
  virtual ~ClockSource()=default;
  virtual uint64_t now() const=0;
};
// All methods run on the Core owner after ordinary control/engine events.
// submit/execute/result form one synchronous owner transaction.
class CorePort {
public:
  virtual ~CorePort()=default;
  virtual bool open(Session)=0;
  virtual void close()=0;
  virtual Status submit(const Command&,uint64_t)=0;
  virtual bool execute()=0;
  virtual Status result(Session,uint64_t) const=0;
  virtual bool sample(uint64_t,wire::DisplaySnapshot&) const=0;
};
struct Policy {
  static constexpr uint64_t HandshakeUs=2000000, IoUs=250000,
    PingEveryUs=1000000, CoreSilenceUs=2000000, SampleEveryUs=50000,
    CommandLifeUs=250000; // provisional host cadence, not sporting tolerances
};
static_assert(Policy::CommandLifeUs<=CommandGate::MaxFutureUs);
class KeyMac final : public wire::Mac {
public:
  explicit KeyMac(const handshake::Crypto& crypto):crypto_(crypto){}
  // Borrow only from the established handshake; decoder is destroyed before
  // the handshake wipes these keys. No extra long-lived key copies.
  void use(const handshake::Secret* key){key_=key;}
  bool sign(const uint8_t* p,size_t n,uint8_t* tag) const override {
    handshake::Tag t{};
    if(!key_ || !crypto_.hmac(*key_,p,n,t)) return false;
    std::memcpy(tag,t.data(),t.size());return true;
  }
  bool verify(const uint8_t* p,size_t n,const uint8_t* tag) const override {
    handshake::Tag t{};
    return sign(p,n,t.data()) && crypto_.equal(t.data(),tag,t.size());
  }
private:
  const handshake::Crypto& crypto_;
  const handshake::Secret* key_=nullptr;
};

// One connected socket/attempt, never reused. External owner must admit at
// most one Core candidate (including handshake), reject additional sockets
// without retiring the admitted peer, and construct a new attempt on reconnect.
// Persistent health::Client is owned by the Taster renderer, not the socket.
// Provider, stream, credential storage, clock, port/client and allocator
// outlive this object. No application frame is accepted before mutual proof.
class Peer {
public:
  Peer(Stream& stream,const ClockSource& clock,const handshake::Crypto& crypto,
       const handshake::PairId& pair,const handshake::Secret& psk,
       CorePort& core,handshake::LinkIds& ids)
    :stream_(stream),clock_(clock),pair_(pair),psk_(psk),core_(&core),ids_(&ids),
     handshake_(handshake::Handshake::Role::Core,crypto),sendMac_(crypto),recvMac_(crypto){}
  Peer(Stream& stream,const ClockSource& clock,const handshake::Crypto& crypto,
       const handshake::PairId& pair,const handshake::Secret& psk,health::Client& client)
    :stream_(stream),clock_(clock),pair_(pair),psk_(psk),client_(&client),
     handshake_(handshake::Handshake::Role::Taster,crypto),sendMac_(crypto),recvMac_(crypto){}
  ~Peer(){close();}
  Peer(const Peer&)=delete;
  Peer& operator=(const Peer&)=delete;
  bool start(){
    if(started_ || closed_) return false;
    started_=true;startedAt_=lastNow_=clock_.now();
    if(!handshake::nonzero(pair_) || !handshake::nonzero(psk_)) return fail();
    if(core_) rxNeed_=handshake::HelloSize;
    else {
      if(client_->connected()) return fail();
      handshake::Packet p;
      if(!handshake_.beginTaster(pair_,p) || !queue(p.bytes.data(),p.length,lastNow_)) return fail();
      rxNeed_=handshake::ChallengeSize;
    }
    return true;
  }
  bool alive() const {return started_ && !closed_;}
  bool established() const {return active_ && alive();}
  Session session() const {return established()?session_:Session{};}
  void close(){
    if(closed_) return;
    closed_=true;
    if(active_) {
      if(core_) core_->close();else client_->disconnect();
    }
    active_=false;session_={};
    decoder_.reset();sendMac_.use(nullptr);recvMac_.use(nullptr);
    handshake_.abort();stream_.close();txLength_=txUsed_=rxUsed_=0;
  }
  // Exactly one bounded read OR write call, at most one decoded frame or
  // handshake step per poll. No socket wait, drain loop or transmit backlog.
  // Call even during silence, and service the normal owner loop first.
  bool poll(){
    if(!alive()) return false;
    uint64_t now;
    if(!time(now)) return fail();
    if(!active_ && now-startedAt_>=Policy::HandshakeUs) return fail();
    if(active_ && core_ && now-lastPing_>=Policy::CoreSilenceUs) return fail();
    if(active_ && client_ && !client_->tick(now)) return fail();
    if(rxUsed_ && now-rxSince_>=Policy::IoUs) return fail();
    if(txLength_) {
      if(now-txSince_>=Policy::IoUs) return fail();
      const size_t remaining=txLength_-txUsed_;
      const int n=stream_.write(tx_.data()+txUsed_,remaining);
      if(n<0 || static_cast<size_t>(n)>remaining) return fail();
      txUsed_+=static_cast<size_t>(n);
      if(txUsed_==txLength_) {
        txLength_=txUsed_=0;
        if(handshake_.stage()==handshake::Stage::FinishPending) {
          if(!handshake_.finishSent() || !activate()) return fail();
        }
      }
      return true;
    }
    const size_t remaining=rxNeed_-rxUsed_;
    if(!remaining || rxNeed_>rx_.size()) return fail();
    const int n=stream_.read(rx_.data()+rxUsed_,remaining);
    if(n<0 || static_cast<size_t>(n)>remaining) return fail();
    if(n>0) {
      if(!rxUsed_) rxSince_=now;
      rxUsed_+=static_cast<size_t>(n);
      if(rxUsed_==rxNeed_) {
        if(!active_) {
          handshake::Packet p;
          if(rxNeed_>p.bytes.size()) return fail();
          p.length=rxNeed_;std::memcpy(p.bytes.data(),rx_.data(),p.length);
          rxUsed_=0;
          return handshakeStep(p) || fail();
        }
        // Read exactly a header, then the declared bounded frame. Feed the
        // reviewed stream decoder once, in order, only on complete record.
        if(rxNeed_==wire::HeaderSize) {
          wire::Frame h;
          if(!wire::readHeader(rx_.data(),h)) return fail();
          rxNeed_=wire::HeaderSize+h.length+wire::TagSize;
          return true;
        }
        wire::Frame f;
        for(size_t i=0;i<rxNeed_;++i) {
          const auto result=decoder_->push(rx_[i],f);
          if(result!=(i+1==rxNeed_?wire::Read::Ready:wire::Read::NeedMore)) return fail();
        }
        rxUsed_=0;rxNeed_=wire::HeaderSize;
        return dispatch(f) || fail();
      }
    }
    if(!active_ || rxUsed_) return true;
    if(core_) {
      if(havePing_ && (sampleDue_ || now-lastSample_>=Policy::SampleEveryUs)) {
        wire::DisplaySnapshot s{};
        if(!time(now) || !core_->sample(now,s) || !(s.session==session_) || s.coreTimeUs!=now ||
           !send(wire::displayState(s,sendSeq_),now)) return fail();
        lastSample_=now;sampleDue_=false;
      }
    } else if(!havePing_ || now-lastPing_>=Policy::PingEveryUs) {
      wire::Ping p;
      if(!time(now) || !client_->startPing(now,p) ||
         !send(wire::ping(session_,sendSeq_,p),now)) return fail();
      lastPing_=now;havePing_=true;
    }
    return true;
  }
  // Renderer calls only following its own button-specific validated physical
  // release/hold/confirmation. Health policy still requires actual neutral
  // and fresh touch. True means queued locally, never sporting acceptance.
  bool command(Action action,Mode mode=Mode::None,uint16_t value=0,bool confirmed=false,
               uint64_t* issuedId=nullptr){
    if(!established() || !client_) return false;
    uint64_t now;
    if(!time(now)) return fail();
    if(!client_->tick(now)) return fail();
    if(txLength_) {client_->cancelTouch();return false;}
    const auto v=client_->view(now);
    uint64_t expires;
    if(!v.mayControl || !health::sum(v.coreNow.lower,Policy::CommandLifeUs,expires) ||
       expires<=v.coreNow.upper) {client_->cancelTouch();return false;}
    if(commandId_==UINT64_MAX) return fail();
    Context c{};
    if(!client_->releaseForCommand(now,commandId_+1,c)) return false;
    Command q{c.session,++commandId_,c.authorityGeneration,c.lifecycleRevision,c.raceId,
              c.proposalRevision,expires,action,mode,value,confirmed};
    if(!send(wire::command(q,sendSeq_),now)) return fail();
    if(issuedId) *issuedId=q.id;
    return true;
  }
private:
  bool fail(){close();return false;}
  bool time(uint64_t& now){
    now=clock_.now();if(now<lastNow_) return false;lastNow_=now;return true;
  }
  bool queue(const uint8_t* p,size_t n,uint64_t now){
    if(txLength_ || !n || n>tx_.size()) return false;
    std::memcpy(tx_.data(),p,n);txLength_=n;txUsed_=0;txSince_=now;return true;
  }
  bool send(const wire::Frame& f,uint64_t now){
    if(!sendSeq_ || f.sequence!=sendSeq_ || !(f.session==session_)) return false;
    wire::Packet p;
    if(!wire::seal(f,core_?wire::Direction::ToTaster:wire::Direction::ToCore,sendMac_,p) ||
       !queue(p.bytes.data(),p.length,now)) return false;
    sendSeq_=sendSeq_==UINT64_MAX?0:sendSeq_+1;
    return true;
  }
  bool handshakeStep(const handshake::Packet& p){
    uint64_t now;if(!time(now) || now-startedAt_>=Policy::HandshakeUs) return false;
    handshake::Packet out;
    using S=handshake::Stage;
    switch(handshake_.stage()){
      case S::Fresh:
        if(!core_ || !handshake_.acceptHello(p,pair_,psk_,*ids_,out)) return false;
        rxNeed_=handshake::ProofSize;break;
      case S::AwaitChallenge:
        if(core_ || !handshake_.acceptChallenge(p,psk_,out)) return false;
        rxNeed_=handshake::ProofSize;break;
      case S::AwaitProof:
        if(!core_ || !handshake_.acceptProof(p,out)) return false;
        break;
      case S::AwaitFinish:
        return !core_ && handshake_.acceptFinish(p) && activate();
      default:return false;
    }
    return queue(out.bytes.data(),out.length,now);
  }
  bool activate(){
    uint64_t now;
    if(!time(now) || now-startedAt_>=Policy::HandshakeUs || !handshake_.established()) return false;
    const auto* keys=handshake_.keys();
    session_=handshake_.session();
    if(!keys || !session_.valid()) return false;
    if(core_ ? !core_->open(session_) : !client_->open(session_,now)) return false;
    active_=true;rxUsed_=0;rxNeed_=wire::HeaderSize;lastPing_=now;
    sendMac_.use(core_?&keys->toTaster:&keys->toCore);
    recvMac_.use(core_?&keys->toCore:&keys->toTaster);
    decoder_.emplace(session_,core_?wire::Direction::ToCore:wire::Direction::ToTaster,recvMac_);
    return true;
  }
  bool dispatch(const wire::Frame& f){
    uint64_t now;if(!time(now)) return false;
    if(core_) {
      if(f.kind==wire::Kind::Ping) {
        wire::Ping p;if(!wire::readPing(f,p)) return false;
        const uint64_t received=now;
        if(!time(now)) return false;
        // Conservative timestamp points: receipt after decoding, send before
        // sealing/first write. Queueing time remains inside the round trip.
        if(!send(wire::pong(session_,sendSeq_,{p.token,p.tasterSendUs,received,now}),now)) return false;
        havePing_=sampleDue_=true;lastPing_=now;return true;
      }
      if(f.kind!=wire::Kind::Command || !havePing_) return false;
      Command q;if(!wire::readCommand(f,q)) return false;
      auto status=core_->submit(q,now);
      if(status==Status::Queued) {
        core_->execute();status=core_->result(q.session,q.id);
      }
      if(status==Status::Queued || status==Status::InFlight) return false;
      if(!time(now) || !send(wire::result(session_,sendSeq_,{q.id,status}),now)) return false;
      sampleDue_=true;return true;
    }
    if(f.kind==wire::Kind::Pong) {
      wire::Pong p;return wire::readPong(f,p) && client_->acceptPong(session_,p,now);
    }
    if(f.kind==wire::Kind::DisplayState) {
      wire::DisplaySnapshot s;return wire::readDisplayState(f,s) &&
        client_->acceptSnapshot(s,now);
    }
    if(f.kind==wire::Kind::Result) {
      wire::Result r;
      if(!wire::readResult(f,r)) return false;
      // A takeover snapshot may already have retired pending knowledge.
      // Unmatched results never restore control or update the display.
      client_->acceptResult(session_,r,now);return client_->connected();
    }
    // ControlState is not sufficient for this display-only consumer.
    return false;
  }
  Stream& stream_;const ClockSource& clock_;
  const handshake::PairId& pair_;const handshake::Secret& psk_;
  CorePort* core_=nullptr;handshake::LinkIds* ids_=nullptr;health::Client* client_=nullptr;
  handshake::Handshake handshake_;KeyMac sendMac_,recvMac_;
  std::optional<wire::StreamDecoder> decoder_;
  Session session_{};
  std::array<uint8_t,wire::MaxFrame> tx_{},rx_{};
  size_t txLength_=0,txUsed_=0,rxUsed_=0,rxNeed_=0;
  uint64_t startedAt_=0,lastNow_=0,txSince_=0,rxSince_=0,lastPing_=0,lastSample_=0,
    sendSeq_=1,commandId_=0;
  bool started_=false,closed_=false,active_=false,havePing_=false,sampleDue_=false;
};
} // namespace pp::taster_link::transport
