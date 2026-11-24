#pragma once
#include "taster_link_bootstrap.h"
#include "taster_link_socket_runtime.h"
#include <mbedtls/version.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_ciphersuites.h>
#include <mbedtls/entropy.h>
#include <mbedtls/pk.h>
#include <mbedtls/ecp.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/bignum.h>

#if MBEDTLS_VERSION_MAJOR != 2 && MBEDTLS_VERSION_MAJOR != 3
#error "Review TLS API before using another Mbed TLS major version"
#endif
#if !defined(MBEDTLS_SSL_PROTO_TLS1_2) || !defined(MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED) || !defined(MBEDTLS_GCM_C) || !defined(MBEDTLS_X509_CRT_WRITE_C) || !defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED)
#error "First-use TLS needs TLS 1.2 ECDHE-ECDSA AES-GCM and P-256 X.509 generation"
#endif

namespace pp::taster_link::bootstrap {
namespace sr=socket_runtime;
enum class TlsState:uint8_t {Fresh,Negotiating,Secure,Closed,Fault};

// Shared Mbed TLS implementation; ESP uses the SDK fork, lwIP SocketStream
// and EspCrypto hardware entropy provider. Linux acceptance uses upstream
// library and real sockets. No network service/listener/firmware activation.
// Owner must reserve exclusive first-use admission BEFORE TCP acceptance/
// connect and verify AP/STA interface, selected BSSID and network generation.
// Keep owner, crypto and admission alive; no asynchronous callbacks. Where
// enabled, platform must initialise PSA crypto before constructing channels.
// Certificate generation, setup and a handshake step may allocate/perform
// expensive crypto; never invoke on the timing/active-race critical path.
// Resource approval and actual latency evidence required before activation.
class MbedFirstUseChannel final:public EncryptedChannel {
public:
  static constexpr uint64_t NegotiationUs=10000000;
  MbedFirstUseChannel(const hs::Crypto& crypto,const Admission& admission)
    :crypto_(crypto),admission_(admission){
    mbedtls_ssl_init(&ssl_);mbedtls_ssl_config_init(&config_);
    mbedtls_pk_init(&key_);mbedtls_x509_crt_init(&cert_);
  }
  ~MbedFirstUseChannel() override{close();}
  MbedFirstUseChannel(const MbedFirstUseChannel&)=delete;
  MbedFirstUseChannel& operator=(const MbedFirstUseChannel&)=delete;
  TlsState state() const{return state_;}
  // Own the supplied connected socket on success OR failure. One use only.
  // Caller must relinquish fd, not share it or supply it again. Never adopt
  // a listener, unconnected socket, sporting peer or external HTTP socket.
  bool start(int fd,pv::Role role,uint64_t now){
    if(state_!=TlsState::Fresh){sr::closeFd(fd);return false;}
    if(!stream_.adopt(fd) || !pv::validRole(role) || !admission_.permitted())return fail();
    const int endpoint=role==pv::Role::Core?MBEDTLS_SSL_IS_SERVER:MBEDTLS_SSL_IS_CLIENT;
    if(mbedtls_ssl_config_defaults(&config_,endpoint,MBEDTLS_SSL_TRANSPORT_STREAM,
                                   MBEDTLS_SSL_PRESET_DEFAULT)!=0)return fail();
#if MBEDTLS_VERSION_MAJOR == 3
    mbedtls_ssl_conf_min_tls_version(&config_,MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&config_,MBEDTLS_SSL_VERSION_TLS1_2);
#else
    mbedtls_ssl_conf_min_version(&config_,MBEDTLS_SSL_MAJOR_VERSION_3,MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_max_version(&config_,MBEDTLS_SSL_MAJOR_VERSION_3,MBEDTLS_SSL_MINOR_VERSION_3);
#endif
    mbedtls_ssl_conf_ciphersuites(&config_,Suites);
    mbedtls_ssl_conf_rng(&config_,&rng,this);
    // Deliberate first-use-only identity limitation: no trust anchor.
    // NEVER use this config for public HTTPS or remembered authentication.
    mbedtls_ssl_conf_authmode(&config_,MBEDTLS_SSL_VERIFY_NONE);
#if defined(MBEDTLS_SSL_RENEGOTIATION)
    mbedtls_ssl_conf_renegotiation(&config_,MBEDTLS_SSL_RENEGOTIATION_DISABLED);
#endif
    mbedtls_ssl_conf_legacy_renegotiation(&config_,MBEDTLS_SSL_LEGACY_NO_RENEGOTIATION);
#if defined(MBEDTLS_SSL_SESSION_TICKETS) && defined(MBEDTLS_SSL_CLI_C)
    if(endpoint==MBEDTLS_SSL_IS_CLIENT)
      mbedtls_ssl_conf_session_tickets(&config_,MBEDTLS_SSL_SESSION_TICKETS_DISABLED);
#endif
    // Fresh config/context every attempt: no session cache, server ticket
    // callbacks, client session import, reset/reuse or TLS 1.3 early data.
    if(endpoint==MBEDTLS_SSL_IS_SERVER &&
       (!makeCertificate() || mbedtls_ssl_conf_own_cert(&config_,&cert_,&key_)!=0))return fail();
    if(!admission_.permitted() || mbedtls_ssl_setup(&ssl_,&config_)!=0)return fail();
    mbedtls_ssl_set_bio(&ssl_,this,&send,&receive,nullptr);
    started_=last_=now;state_=TlsState::Negotiating;return true;
  }
  // A single library handshake step, no loop/drain/wait. Nonblocking BIO
  // callbacks may execute more than once inside a step; this is NOT a
  // measured CPU-time bound. Unknown library errors retire the channel.
  TlsState pollHandshake(uint64_t now){
    if(state_!=TlsState::Negotiating)return state_;
    if(now<last_ || now-started_>=NegotiationUs || !admission_.permitted()){
      fail();return state_;
    }
    last_=now;
    const int rc=mbedtls_ssl_handshake_step(&ssl_);
    if(rc && !wouldBlock(rc)){fail();return state_;}
#if MBEDTLS_VERSION_MAJOR == 3
    const bool over=mbedtls_ssl_is_handshake_over(&ssl_)!=0;
#else
    const bool over=ssl_.state==MBEDTLS_SSL_HANDSHAKE_OVER;
#endif
    if(over){
      const char* suite=mbedtls_ssl_get_ciphersuite(&ssl_);
      const char* version=mbedtls_ssl_get_version(&ssl_);
      if(rc || !suite || !version ||
         std::strcmp(version,"TLSv1.2") ||
         std::strcmp(suite,"TLS-ECDHE-ECDSA-WITH-AES-128-GCM-SHA256") ||
         !admission_.permitted()){fail();return state_;}
      state_=TlsState::Secure;
    }
    return state_;
  }
  bool secure() const override{return state_==TlsState::Secure && admission_.permitted();}
  int read(uint8_t* out,size_t n) override{
    if(!secure() || !out || !n || n>OfferSize){fail();return -1;}
    return result(mbedtls_ssl_read(&ssl_,out,n));
  }
  int write(const uint8_t* in,size_t n) override{
    if(!secure() || !in || !n || n>OfferSize){fail();return -1;}
    // Mbed TLS requires identical args after WANT_READ/WRITE. Exchange
    // preserves the unsent frame pointer/length. Enforce it here as well.
    if(pending_ && (pending_!=in || pendingSize_!=n)){fail();return -1;}
    const int rc=mbedtls_ssl_write(&ssl_,in,n);
    if(wouldBlock(rc)){pending_=in;pendingSize_=n;return 0;}
    pending_=nullptr;pendingSize_=0;
    return result(rc);
  }
  // No blocking close-notify/flush loop. Successful ssl_write means
  // encrypted bytes handed to TCP, not delivery to other endpoint.
  // Immediate socket close may lose final ack; Exchange handles uncertainty.
  void close() override{
    if(state_==TlsState::Closed || state_==TlsState::Fault)return;
    release();state_=TlsState::Closed;
  }
private:
  inline static constexpr int Suites[]={MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,0};
  static bool wouldBlock(int rc){return rc==MBEDTLS_ERR_SSL_WANT_READ || rc==MBEDTLS_ERR_SSL_WANT_WRITE;}
  int result(int rc){
    if(rc>0)return rc;
    if(wouldBlock(rc))return 0;
    fail();return -1;
  }
  static int rng(void* ctx,unsigned char* out,size_t n){
    auto& self=*static_cast<MbedFirstUseChannel*>(ctx);
    return self.admission_.permitted() && self.crypto_.random(out,n)?0:MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  }
  static int send(void* ctx,const unsigned char* in,size_t n){
    auto& self=*static_cast<MbedFirstUseChannel*>(ctx);
    if(!self.admission_.permitted())return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    const int rc=self.stream_.write(in,n);
    return rc>0?rc:rc==0?MBEDTLS_ERR_SSL_WANT_WRITE:MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  }
  static int receive(void* ctx,unsigned char* out,size_t n){
    auto& self=*static_cast<MbedFirstUseChannel*>(ctx);
    if(!self.admission_.permitted())return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    const int rc=self.stream_.read(out,n);
    return rc>0?rc:rc==0?MBEDTLS_ERR_SSL_WANT_READ:MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  }
  bool makeCertificate(){
    const auto* info=mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY);
    if(!info || mbedtls_pk_setup(&key_,info)!=0 ||
       mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1,mbedtls_pk_ec(key_),&rng,this)!=0)return false;
    mbedtls_x509write_cert writer;mbedtls_x509write_crt_init(&writer);
    mbedtls_x509write_crt_set_version(&writer,MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&writer,MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&writer,&key_);
    mbedtls_x509write_crt_set_issuer_key(&writer,&key_);
    std::array<uint8_t,16> serial{};
    bool ok=crypto_.random(serial.data(),serial.size()) && hs::nonzero(serial);
    serial[0]=(serial[0]&0x7f)|0x01;
#if MBEDTLS_VERSION_MAJOR == 3
    ok=ok && mbedtls_x509write_crt_set_serial_raw(&writer,serial.data(),serial.size())==0;
#else
    mbedtls_mpi number;mbedtls_mpi_init(&number);
    ok=ok && mbedtls_mpi_read_binary(&number,serial.data(),serial.size())==0 &&
      mbedtls_x509write_crt_set_serial(&writer,&number)==0;
    mbedtls_mpi_free(&number);
#endif
    // No clock/Internet dependency. This self-signed certificate is
    // merely a transient TLS key, not a trusted identity or validity proof.
    ok=ok && mbedtls_x509write_crt_set_subject_name(&writer,"CN=P&P first use")==0 &&
      mbedtls_x509write_crt_set_issuer_name(&writer,"CN=P&P first use")==0 &&
      mbedtls_x509write_crt_set_validity(&writer,"20200101000000","20491231235959")==0 &&
      mbedtls_x509write_crt_set_basic_constraints(&writer,0,-1)==0 &&
      mbedtls_x509write_crt_set_key_usage(&writer,MBEDTLS_X509_KU_DIGITAL_SIGNATURE)==0;
    std::array<uint8_t,1024> der{};
    const int size=ok?mbedtls_x509write_crt_der(&writer,der.data(),der.size(),&rng,this):-1;
    mbedtls_x509write_crt_free(&writer);
    ok=size>0 && static_cast<size_t>(size)<=der.size() &&
      mbedtls_x509_crt_parse_der(&cert_,der.data()+der.size()-size,static_cast<size_t>(size))==0;
    crypto_.wipe(serial.data(),serial.size());crypto_.wipe(der.data(),der.size());
    return ok;
  }
  void release(){
    pending_=nullptr;pendingSize_=0;stream_.close();
    mbedtls_ssl_free(&ssl_);mbedtls_ssl_config_free(&config_);
    mbedtls_x509_crt_free(&cert_);mbedtls_pk_free(&key_);
  }
  bool fail(){
    if(state_!=TlsState::Fault && state_!=TlsState::Closed){release();state_=TlsState::Fault;}
    return false;
  }
  const hs::Crypto& crypto_;const Admission& admission_;
  sr::SocketStream stream_;mbedtls_ssl_context ssl_{};mbedtls_ssl_config config_{};
  mbedtls_pk_context key_{};mbedtls_x509_crt cert_{};
  TlsState state_=TlsState::Fresh;uint64_t started_=0,last_=0;
  const uint8_t* pending_=nullptr;size_t pendingSize_=0;
};

} // namespace pp::taster_link::bootstrap
