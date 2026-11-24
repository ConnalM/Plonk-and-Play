// Real TLS 1.3 through connected OpenSSL memory BIOs, with bounded partial
// application reads/writes. No ESP TLS, network scheduler or NVS evidence.
#include "pp/taster_link_bootstrap.h"
#include "pp/taster_link_crypto_primitives.h"
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/x509.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <cassert>
#include <iostream>

namespace b=pp::taster_link::bootstrap;
namespace p=pp::taster_link::pairing;
namespace h=pp::taster_link::handshake;
class Crypto final:public h::Crypto {
public:
  bool entropy=true;
  bool random(uint8_t* out,size_t n) const override {
    return entropy && RAND_bytes(out,static_cast<int>(n))==1;
  }
  static bool raw(const uint8_t* k,size_t kn,const uint8_t* msg,size_t n,uint8_t* out){
    unsigned size=0;
    return HMAC(EVP_sha256(),k,static_cast<int>(kn),msg,n,out,&size)!=nullptr && size==32;
  }
  bool hkdf(const h::Secret& key,const uint8_t* salt,size_t sn,
            const uint8_t* info,size_t in,uint8_t* out,size_t on) const override {
    return h::ppthHkdf32(key,salt,sn,info,in,out,on,&raw,[](uint8_t* q,size_t n){OPENSSL_cleanse(q,n);});
  }
  bool hmac(const h::Secret& key,const uint8_t* msg,size_t n,h::Tag& out) const override {
    return raw(key.data(),key.size(),msg,n,out.data());
  }
  bool equal(const uint8_t* a,const uint8_t* c,size_t n) const override{return CRYPTO_memcmp(a,c,n)==0;}
  void wipe(uint8_t* q,size_t n) const override{OPENSSL_cleanse(q,n);}
};
class Store final:public p::Store {
public:
  enum class Fault { None,BeforeWrite,AfterWrite };
  bool present=false;
  unsigned writes=0;
  Fault fault=Fault::None;
  p::Record record{};
  bool protectedReady() const override{return true;}
  p::Read read(p::Record& out) override {
    if(!present)return p::Read::Missing;
    out=record;return p::Read::Found;
  }
  bool replace(const p::Record& in) override {
    ++writes;if(fault==Fault::BeforeWrite)return false;
    record=in;present=true;return fault!=Fault::AfterWrite;
  }
};
struct Owner final:p::Maintenance,b::Admission {
  bool admitted=true,ready=true,quiet=true,authorised=true;
  bool locallyAuthorised() const override{return authorised;}
  bool safeLifecycle() const override{return ready;}
  bool transportQuiescent() const override{return quiet;}
  bool permitted() const override{return admitted && ready && quiet;}
};
class Channel final:public b::EncryptedChannel {
public:
  SSL* ssl=nullptr;
  bool closed=false,enabled=true,stallRead=false,stallWrite=false;
  unsigned reads=0,writes=0;
  bool secure() const override{
    return enabled && !closed && ssl && SSL_is_init_finished(ssl) &&
      SSL_version(ssl)==TLS1_3_VERSION && !SSL_session_reused(ssl) &&
      SSL_CIPHER_is_aead(SSL_get_current_cipher(ssl));
  }
  int read(uint8_t* out,size_t n) override {
    ++reads;if(!secure())return -1;
    if(stallRead)return 0;
    return progress(SSL_read(ssl,out,static_cast<int>(std::min(n,size_t{5}))));
  }
  int write(const uint8_t* in,size_t n) override {
    ++writes;if(!secure())return -1;
    if(stallWrite)return 0;
    return progress(SSL_write(ssl,in,static_cast<int>(std::min(n,size_t{7}))));
  }
  // SSL/BIO objects stay alive until the other side has consumed already
  // queued ciphertext. Fixture only: close disables further local I/O.
  void close() override{closed=true;}
private:
  int progress(int rc) const {
    if(rc>0)return rc;
    const int err=SSL_get_error(ssl,rc);
    return err==SSL_ERROR_WANT_READ || err==SSL_ERROR_WANT_WRITE?0:-1;
  }
};
class Tls {
public:
  Channel core,taster;
  Tls(){
    cc=SSL_CTX_new(TLS_server_method());tc=SSL_CTX_new(TLS_client_method());
    assert(cc && tc);
    for(auto* ctx:{cc,tc}){
      assert(SSL_CTX_set_min_proto_version(ctx,TLS1_3_VERSION)==1);
      assert(SSL_CTX_set_max_proto_version(ctx,TLS1_3_VERSION)==1);
      assert(SSL_CTX_set_ciphersuites(ctx,"TLS_AES_128_GCM_SHA256")==1);
      SSL_CTX_set_session_cache_mode(ctx,SSL_SESS_CACHE_OFF);
      SSL_CTX_set_options(ctx,SSL_OP_NO_TICKET);
      assert(SSL_CTX_set_max_early_data(ctx,0)==1);
    }
    // Fresh self-signed certificate. First-use TOFU intentionally has no
    // independent trust anchor. Do NOT use SSL_VERIFY_NONE for remembered
    // authentication or ordinary public HTTPS.
    SSL_CTX_set_verify(tc,SSL_VERIFY_NONE,nullptr);
    auto* gen=EVP_PKEY_CTX_new_id(EVP_PKEY_EC,nullptr);assert(gen);
    EVP_PKEY* key=nullptr;
    assert(EVP_PKEY_keygen_init(gen)>0 &&
      EVP_PKEY_CTX_set_ec_paramgen_curve_nid(gen,NID_X9_62_prime256v1)>0 &&
      EVP_PKEY_keygen(gen,&key)>0);
    EVP_PKEY_CTX_free(gen);
    X509* cert=X509_new();assert(cert);
    assert(X509_set_version(cert,2)==1);
    assert(ASN1_INTEGER_set(X509_get_serialNumber(cert),1)==1);
    assert(X509_gmtime_adj(X509_getm_notBefore(cert),-60));
    assert(X509_gmtime_adj(X509_getm_notAfter(cert),86400));
    assert(X509_set_pubkey(cert,key)==1);
    auto* name=X509_get_subject_name(cert);
    assert(X509_NAME_add_entry_by_txt(name,"CN",MBSTRING_ASC,
      reinterpret_cast<const unsigned char*>("P&P host first-use fixture"),-1,-1,0)==1);
    assert(X509_set_issuer_name(cert,name)==1 && X509_sign(cert,key,EVP_sha256())>0);
    assert(SSL_CTX_use_certificate(cc,cert)==1 && SSL_CTX_use_PrivateKey(cc,key)==1);
    X509_free(cert);EVP_PKEY_free(key);
    core.ssl=SSL_new(cc);taster.ssl=SSL_new(tc);assert(core.ssl && taster.ssl);
    BIO *a=nullptr,*c=nullptr;
    assert(BIO_new_bio_pair(&a,8192,&c,8192)==1);
    SSL_set_bio(core.ssl,a,a);SSL_set_bio(taster.ssl,c,c);
    SSL_set_accept_state(core.ssl);SSL_set_connect_state(taster.ssl);
    for(unsigned i=0;i<1000 && (!core.secure() || !taster.secure());++i){
      advance(core.ssl);advance(taster.ssl);
    }
    assert(core.secure() && taster.secure());
  }
  ~Tls(){SSL_free(core.ssl);SSL_free(taster.ssl);SSL_CTX_free(cc);SSL_CTX_free(tc);}
  Tls(const Tls&)=delete;Tls& operator=(const Tls&)=delete;
private:
  SSL_CTX *cc=nullptr,*tc=nullptr;
  static void advance(SSL* ssl){
    if(SSL_is_init_finished(ssl))return;
    const int rc=SSL_do_handshake(ssl);
    if(rc==1)return;
    const int e=SSL_get_error(ssl,rc);
    assert(e==SSL_ERROR_WANT_READ || e==SSL_ERROR_WANT_WRITE);
  }
};
static bool authenticates(const p::Material& c,const p::Material& t,const Crypto& crypto,h::LinkIds& ids){
  h::Handshake core(h::Handshake::Role::Core,crypto),taster(h::Handshake::Role::Taster,crypto);
  h::Packet hello,challenge,proof,finish;
  return taster.beginTaster(t.pair(),hello) && core.acceptHello(hello,c.pair(),c.psk(),ids,challenge) &&
    taster.acceptChallenge(challenge,t.psk(),proof) && core.acceptProof(proof,finish) &&
    core.finishSent() && taster.acceptFinish(finish) && core.session()==taster.session() &&
    core.keys()->toCore==taster.keys()->toCore && core.keys()->toTaster==taster.keys()->toTaster;
}
struct Rig {
  Crypto crypto;Owner co,to;Store cs,ts;
  p::Vault cv{p::Role::Core,cs,crypto,co},tv{p::Role::Taster,ts,crypto,to};
  Tls tls;
  b::Exchange c{p::Role::Core,cv,crypto,tls.core,co},t{p::Role::Taster,tv,crypto,tls.taster,to};
  uint64_t now=1000;
  Rig(){assert(cv.load()==p::State::Empty && tv.load()==p::State::Empty);}
  void start(){assert(c.start(now) && t.start(now));}
  void step(){
    now+=1000;
    const auto ci=tls.core.reads+tls.core.writes,ti=tls.taster.reads+tls.taster.writes;
    c.poll(now);t.poll(now);
    assert(tls.core.reads+tls.core.writes<=ci+1 && tls.taster.reads+tls.taster.writes<=ti+1);
  }
  template<class Pred>void until(Pred pred){
    for(unsigned i=0;i<12000 && !pred();++i)step();
    assert(pred());
  }
  void finish(){until([&]{return c.outcome()!=b::Outcome::Running && t.outcome()!=b::Outcome::Running;});}
};
int main(){
  using O=b::Outcome;using S=p::State;
  {
    Rig r;r.start();r.finish();
    assert(r.c.outcome()==O::Complete && r.t.outcome()==O::Complete);
    assert(r.cv.state()==S::Paired && r.tv.state()==S::Paired &&
      r.cs.writes==1 && r.ts.writes==1);
    assert(r.cv.active()->pair()==r.tv.active()->pair() &&
      r.cv.active()->psk()==r.tv.active()->psk());
    p::Vault rebootCore(p::Role::Core,r.cs,r.crypto,r.co),rebootTaster(p::Role::Taster,r.ts,r.crypto,r.to);
    assert(rebootCore.load()==S::Paired && rebootTaster.load()==S::Paired);
    h::LinkIds ids(19);
    assert(authenticates(*rebootCore.active(),*rebootTaster.active(),r.crypto,ids));
    Tls second;
    b::Exchange reject(p::Role::Core,rebootCore,r.crypto,second.core,r.co);
    assert(!reject.start(r.now) && reject.outcome()==O::Failed && !second.core.reads && !second.core.writes);
    assert(r.cs.writes==1);
    b::Exchange rejectTaster(p::Role::Taster,rebootTaster,r.crypto,second.taster,r.to);
    assert(!rejectTaster.start(r.now) && r.ts.writes==1);
  }
  {
    Rig r;r.tls.core.enabled=false;
    assert(!r.c.start(r.now) && !r.cs.writes && !r.tls.core.reads && !r.tls.core.writes);
  }
  {
    Rig r;r.co.admitted=false;
    assert(!r.c.start(r.now) && !r.cs.writes);
  }
  {
    Rig r;r.co.quiet=false;
    assert(!r.c.start(r.now) && !r.cs.writes);
  }
  {
    Rig r;b::Exchange wrong(p::Role::Taster,r.cv,r.crypto,r.tls.core,r.co);
    assert(!wrong.start(r.now) && !r.cs.writes);
  }
  {
    Rig r;r.crypto.entropy=false;r.start();r.finish();
    assert(r.c.outcome()==O::Failed && r.t.outcome()==O::Failed && !r.cs.writes && !r.ts.writes);
  }
  {
    Rig r;r.start();
    // Let Taster commit then withdraw Core admission before reading the ack.
    r.until([&]{return r.tv.state()==S::Paired;});
    assert(r.cv.state()==S::Empty);
    r.co.ready=false;r.finish();
    assert(r.c.outcome()==O::Failed && r.t.outcome()==O::Uncertain);
    assert(!r.cs.writes && r.ts.writes==1);
  }
  for(auto failure:{Store::Fault::BeforeWrite,Store::Fault::AfterWrite}){
    {
      Rig r;r.ts.fault=failure;r.start();r.finish();
      assert(r.t.outcome()==O::Uncertain && r.c.outcome()==O::Failed);
      assert(r.tv.state()==S::Fault && r.cv.state()==S::Empty && !r.cs.writes);
    }
    {
      Rig r;r.cs.fault=failure;r.start();r.finish();
      assert(r.c.outcome()==O::Uncertain && r.t.outcome()==O::Uncertain);
      assert(r.cv.state()==S::Fault && r.tv.state()==S::Paired);
    }
  }
  {
    Rig r;r.start();
    r.until([&]{return r.cv.state()==S::Paired;});
    // Core has committed but final acknowledgement cannot be delivered.
    r.tls.core.stallWrite=true;r.finish();
    assert(r.c.outcome()==O::Uncertain && r.t.outcome()==O::Uncertain);
    assert(r.cv.active() && r.tv.active() && r.cs.writes==1 && r.ts.writes==1);
    h::LinkIds ids(20);
    assert(authenticates(*r.cv.active(),*r.tv.active(),r.crypto,ids));
  }
  {
    Rig r;r.start();r.c.poll(r.now-1);
    assert(r.c.outcome()==O::Failed && !r.cs.writes);
  }
  {
    Rig r;assert(r.c.start(r.now));
    b::Frame invalid{};b::prefix(invalid,b::Kind::Offer);
    assert(r.tls.taster.write(invalid.data(),b::Header)==static_cast<int>(b::Header));
    r.until([&]{return r.c.outcome()!=O::Running;});
    assert(r.c.outcome()==O::Failed && !r.cs.writes);
  }
  {
    // Vault's maintenance permission is a separate fence from admission.
    Rig r;r.to.authorised=false;r.start();r.finish();
    assert(r.t.outcome()==O::Uncertain && !r.ts.writes && !r.cs.writes);
  }
  std::cout<<"S3g2e first-use TLS exchange, vault commits, subsequent PPTH and interruption PASS\n";
}
