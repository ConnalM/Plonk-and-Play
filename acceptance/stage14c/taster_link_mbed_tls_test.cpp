// Real Mbed TLS first-use channel over Linux TCP sockets. This exercises
// the shared adapter, not the ESP-IDF fork, Wi-Fi timing or encrypted NVS.
#include "pp/taster_link_mbed_tls.h"
#include "pp/taster_link_crypto_primitives.h"
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <cassert>
#include <iostream>
#include <thread>

namespace b=pp::taster_link::bootstrap;
namespace p=pp::taster_link::pairing;
namespace h=pp::taster_link::handshake;
namespace sr=pp::taster_link::socket_runtime;

class Crypto final:public h::Crypto {
public:
  bool entropy=true;
  bool random(uint8_t* out,size_t n) const override {
    return entropy && n && RAND_bytes(out,static_cast<int>(n))==1;
  }
  static bool raw(const uint8_t* k,size_t kn,const uint8_t* msg,size_t n,uint8_t* out){
    unsigned size=0;
    return HMAC(EVP_sha256(),k,static_cast<int>(kn),msg,n,out,&size)!=nullptr && size==32;
  }
  bool hkdf(const h::Secret& key,const uint8_t* salt,size_t sn,
            const uint8_t* info,size_t in,uint8_t* out,size_t on) const override{
    return h::ppthHkdf32(key,salt,sn,info,in,out,on,&raw,
      [](uint8_t* q,size_t n){OPENSSL_cleanse(q,n);});
  }
  bool hmac(const h::Secret& key,const uint8_t* msg,size_t n,h::Tag& out) const override {
    return raw(key.data(),key.size(),msg,n,out.data());
  }
  bool equal(const uint8_t* a,const uint8_t* c,size_t n) const override{return CRYPTO_memcmp(a,c,n)==0;}
  void wipe(uint8_t* q,size_t n) const override{OPENSSL_cleanse(q,n);}
};
class Store final:public p::Store {
public:
  bool present=false;unsigned writes=0;p::Record record{};
  bool protectedReady() const override{return true;}
  p::Read read(p::Record& out) override{
    if(!present)return p::Read::Missing;
    out=record;return p::Read::Found;
  }
  bool replace(const p::Record& in) override{
    record=in;present=true;++writes;return true;
  }
};
struct Owner final:p::Maintenance,b::Admission{
  bool allowed=true;
  bool locallyAuthorised() const override{return allowed;}
  bool safeLifecycle() const override{return allowed;}
  bool transportQuiescent() const override{return allowed;}
  bool permitted() const override{return allowed;}
};
struct Fds{
  int core=-1,taster=-1;
  Fds(){
    int l=::socket(AF_INET,SOCK_STREAM,0);assert(l>=0);
    auto addr=sr::address(htonl(INADDR_LOOPBACK),0);
    assert(::bind(l,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==0);
    assert(::listen(l,1)==0);
    socklen_t len=sizeof(addr);assert(::getsockname(l,reinterpret_cast<sockaddr*>(&addr),&len)==0);
    taster=::socket(AF_INET,SOCK_STREAM,0);assert(taster>=0);
    assert(::connect(taster,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==0);
    core=::accept(l,nullptr,nullptr);assert(core>=0);sr::closeFd(l);
  }
  ~Fds(){sr::closeFd(core);sr::closeFd(taster);}
  Fds(const Fds&)=delete;Fds& operator=(const Fds&)=delete;
  int takeCore(){const int fd=core;core=-1;return fd;}
  int takeTaster(){const int fd=taster;taster=-1;return fd;}
};
struct Rig {
  Crypto crypto;Owner co,to;Store cs,ts;
  p::Vault cv{p::Role::Core,cs,crypto,co},tv{p::Role::Taster,ts,crypto,to};
  Fds fds;
  b::MbedFirstUseChannel c{crypto,co},t{crypto,to};
  uint64_t now=100;
  Rig(){assert(cv.load()==p::State::Empty && tv.load()==p::State::Empty);}
  void start(){
    assert(c.start(fds.takeCore(),p::Role::Core,now));
    assert(t.start(fds.takeTaster(),p::Role::Taster,now));
    assert(!c.secure() && !t.secure());
  }
  void negotiate(){
    for(unsigned i=0;i<5000 && (!c.secure() || !t.secure());++i){
      now+=1000;c.pollHandshake(now);t.pollHandshake(now);
      assert(c.state()!=b::TlsState::Fault && t.state()!=b::TlsState::Fault);
      std::this_thread::yield();
    }
    assert(c.secure() && t.secure());
  }
};
static bool authenticates(const p::Material& c,const p::Material& t,const Crypto& crypto){
  h::Handshake core(h::Handshake::Role::Core,crypto),taster(h::Handshake::Role::Taster,crypto);
  h::LinkIds ids(42);h::Packet hello,challenge,proof,finish;
  return taster.beginTaster(t.pair(),hello) && core.acceptHello(hello,c.pair(),c.psk(),ids,challenge) &&
    taster.acceptChallenge(challenge,t.psk(),proof) && core.acceptProof(proof,finish) &&
    core.finishSent() && taster.acceptFinish(finish) &&
    core.keys()->toCore==taster.keys()->toCore && core.keys()->toTaster==taster.keys()->toTaster;
}
int main(){
  using T=b::TlsState;using O=b::Outcome;
  {
    Rig r;r.start();r.negotiate();
    b::Exchange core(p::Role::Core,r.cv,r.crypto,r.c,r.co),
                taster(p::Role::Taster,r.tv,r.crypto,r.t,r.to);
    assert(core.start(r.now) && taster.start(r.now));
    for(unsigned i=0;i<20000 && (core.outcome()==O::Running || taster.outcome()==O::Running);++i){
      r.now+=1000;core.poll(r.now);taster.poll(r.now);
      std::this_thread::yield();
    }
    assert(core.outcome()==O::Complete && taster.outcome()==O::Complete);
    assert(r.cs.writes==1 && r.ts.writes==1 && r.c.state()==T::Closed && r.t.state()==T::Closed);
    p::Vault rc(p::Role::Core,r.cs,r.crypto,r.co),rt(p::Role::Taster,r.ts,r.crypto,r.to);
    assert(rc.load()==p::State::Paired && rt.load()==p::State::Paired);
    assert(authenticates(*rc.active(),*rt.active(),r.crypto));
  }
  {
    Rig r;assert(r.c.start(r.fds.takeCore(),p::Role::Core,r.now));
    assert(r.c.pollHandshake(r.now+b::MbedFirstUseChannel::NegotiationUs)==T::Fault);
    assert(!r.c.secure() && !r.cs.writes);
    r.c.close();assert(r.c.state()==T::Fault);
  }
  {
    Rig r;r.start();r.co.allowed=false;
    assert(r.c.pollHandshake(++r.now)==T::Fault && !r.cs.writes);
  }
  {
    Rig r;r.start();assert(r.c.pollHandshake(r.now-1)==T::Fault && !r.c.secure());
  }
  {
    Rig r;r.crypto.entropy=false;
    assert(!r.c.start(r.fds.takeCore(),p::Role::Core,r.now) && r.c.state()==T::Fault);
  }
  {
    Rig r;r.co.allowed=false;const int fd=r.fds.takeCore();
    assert(!r.c.start(fd,p::Role::Core,r.now));
    assert(::fcntl(fd,F_GETFD)==-1 && errno==EBADF);
  }
  {
    Rig r;assert(!r.c.start(r.fds.takeCore(),static_cast<p::Role>(0),r.now));
    assert(r.c.state()==T::Fault);
  }
  {
    Rig r;r.start();uint8_t byte=1;
    assert(r.c.write(&byte,1)<0 && r.c.state()==T::Fault && !r.cs.writes);
  }
  {
    Rig r;assert(r.c.start(r.fds.takeCore(),p::Role::Core,r.now));
    // A complete bogus TLS record header + payload is not a first-use
    // request, and cannot enable the application channel.
    const uint8_t bogus[]={0x16,0x03,0x03,0,4,0xff,0,0,0};
    assert(::send(r.fds.taster,bogus,sizeof(bogus),0)==static_cast<ssize_t>(sizeof(bogus)));
    for(unsigned i=0;i<1000 && r.c.state()==T::Negotiating;++i){
      r.now+=1000;r.c.pollHandshake(r.now);std::this_thread::yield();
    }
    assert(r.c.state()==T::Fault && !r.c.secure() && !r.cs.writes);
  }
  {
    Rig r;r.start();r.negotiate();
    r.to.allowed=false;assert(!r.t.secure());
    uint8_t out=0;assert(r.t.read(&out,1)<0 && r.t.state()==T::Fault);
  }
  {
    Rig r;r.start();r.negotiate();r.c.close();
    uint8_t out=0;int n=0;
    for(unsigned i=0;i<10000 && n==0;++i){n=r.t.read(&out,1);std::this_thread::yield();}
    assert(n<0 && r.t.state()==T::Fault);
  }
  {
    Rig r;r.start();r.negotiate();
    std::array<uint8_t,b::OfferSize+1> oversized{};
    assert(r.c.write(oversized.data(),oversized.size())<0 && r.c.state()==T::Fault);
  }
  std::cout<<"S3g2f Mbed TLS "<<MBEDTLS_VERSION_STRING
           <<" real TCP, first-use vault exchange and subsequent PPTH PASS\n";
}
