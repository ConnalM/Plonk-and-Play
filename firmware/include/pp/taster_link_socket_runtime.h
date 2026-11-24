#pragma once
#include "taster_link_transport.h"
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#if defined(ESP_PLATFORM)
#include "sdkconfig.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#endif

namespace pp::taster_link::socket_runtime {

// This source is shared by the Linux socket acceptance and the opt-in
// ESP/lwIP adapter. Nonblocking BSD socket calls are not a measured bound
// on ESP scheduler or TCP/IP-task latency. No firmware entry point here.
namespace tr=transport;
struct Budget {
  unsigned total=0,reservedOther=0;
  static constexpr unsigned ExistingBrowserFloor=10;
  // Listener + admitted peer + one transient accept/reject descriptor.
  // Minimum only: reserve additional services and margin before enabling.
  bool permits() const {
    unsigned available=total;
#if defined(ESP_PLATFORM)
#if defined(CONFIG_LWIP_MAX_SOCKETS)
    if(available>CONFIG_LWIP_MAX_SOCKETS) available=CONFIG_LWIP_MAX_SOCKETS;
#else
    return false;
#endif
#endif
    return reservedOther>=ExistingBrowserFloor && available>=reservedOther &&
      available-reservedOther>=3;
  }
};

// Address values are IPv4 in network byte order, copied by the platform
// owner from the selected Core AP / paired Taster STA network. No DNS,
// discovery by SSID, wildcard listener or automatic Internet/STA fallback.
// Generation must change on actual interface/link reinitialisation, even
// when DHCP returns the same addresses. Wi-Fi events must be marshalled
// to the owner before poll; they may not call or destroy a Peer directly.
struct Network {
  bool ready=false;
  uint64_t generation=0;
  uint32_t local=0,core=0;
  bool same(const Network& n) const {
    return ready==n.ready && generation==n.generation && local==n.local && core==n.core;
  }
};
inline bool usableAddress(uint32_t ip) {
  const uint32_t host=ntohl(ip);
  return host!=0 && host!=UINT32_MAX && (host>>24)!=0 && (host>>24)<224;
}
inline sockaddr_in address(uint32_t ip,uint16_t port) {
  sockaddr_in a{};
  a.sin_family=AF_INET;a.sin_port=htons(port);a.sin_addr.s_addr=ip;return a;
}
inline bool wouldBlock(int error) {
  return error==EAGAIN || error==EWOULDBLOCK || error==EINTR;
}
inline bool nonblocking(int fd) {
  const int flags=::fcntl(fd,F_GETFL,0);
  return flags>=0 && ::fcntl(fd,F_SETFL,flags|O_NONBLOCK)==0;
}
inline bool noDelay(int fd) {
  const int one=1;
  return ::setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof(one))==0;
}
inline void closeFd(int& fd) {
  if(fd>=0) {const int old=fd;fd=-1;(void)::close(old);}
}
class SocketStream final : public tr::Stream {
public:
  SocketStream()=default;
  ~SocketStream() override {close();}
  SocketStream(const SocketStream&)=delete;
  SocketStream& operator=(const SocketStream&)=delete;
  // Takes ownership regardless of success.
  bool adopt(int fd) {
    if(fd<0) return false;
    if(fd_>=0) {closeFd(fd);return false;}
    fd_=fd;
    if(!nonblocking(fd_) || !noDelay(fd_)) {close();return false;}
    return true;
  }
  int fd() const {return fd_;}
  int read(uint8_t* p,size_t n) override {
    if(fd_<0 || !p || !n) return -1;
    const auto count=::recv(fd_,p,n,0);
    if(count>0) return static_cast<int>(count);
    return count<0 && wouldBlock(errno)?0:-1;
  }
  int write(const uint8_t* p,size_t n) override {
    if(fd_<0 || !p || !n) return -1;
#if defined(MSG_NOSIGNAL)
    const auto count=::send(fd_,p,n,MSG_NOSIGNAL);
#else
    const auto count=::send(fd_,p,n,0);
#endif
    if(count>0) return static_cast<int>(count);
    return count<0 && wouldBlock(errno)?0:-1;
  }
  void close() override {closeFd(fd_);}
private:int fd_=-1;
};
class Retry {
public:
  static constexpr uint64_t FirstUs=500000,MaxUs=8000000;
  bool due(uint64_t now) const {return !waiting_ || (now>=since_ && now-since_>=wait_);}
  void failure(uint64_t now) {
    since_=now;wait_=next_;waiting_=true;
    next_=next_>=MaxUs/2?MaxUs:next_*2;
  }
  void reset(){waiting_=false;next_=FirstUs;}
  uint64_t waitUs() const {return waiting_?wait_:0;}
private:
  uint64_t since_=0,wait_=0,next_=FirstUs;
  bool waiting_=false;
};

// One service per Core boot, same boot-scoped LinkIds for its full life.
// Credential refs are securely loaded immutable material, not an SSID or
// password; do not mutate while service exists. Explicit commissioning
// and capacity approval must both be true before any listener opens.
class CoreService {
public:
  CoreService(const tr::ClockSource& clock,const handshake::Crypto& crypto,
              const handshake::PairId& pair,const handshake::Secret& psk,
              tr::CorePort& port,handshake::LinkIds& ids,uint16_t tcpPort,Budget budget)
    :clock_(clock),crypto_(crypto),pair_(pair),psk_(psk),port_(port),ids_(ids),
     tcpPort_(tcpPort),budget_(budget){}
  ~CoreService(){stop();}
  CoreService(const CoreService&)=delete;
  CoreService& operator=(const CoreService&)=delete;
  bool listening() const {return listener_>=0;}
  bool established() const {return peer_ && peer_->established();}
  Session session() const {return established()?peer_->session():Session{};}
  unsigned refused() const {return refused_;}
  void stop() {
    peer_.reset();stream_.close();closeFd(listener_);acceptSeen_=false;
  }
  // Existing race/control/engine/persistence work is serviced first.
  // At most one peer poll and one rate-limited accept per owner cycle.
  // No waiting for a client, draining the listener backlog or retry loops.
  void poll(const Network& network,bool commissioned,bool resourcesApproved) {
    const auto now=clock_.now();
    if(timeSeen_ && now<lastNow_) clockFault_=true;
    timeSeen_=true;lastNow_=now;
    if(!network.same(network_)) {stop();retry_.reset();network_=network;}
    if(clockFault_ || !commissioned || !resourcesApproved || !budget_.permits() ||
       !network.ready || !network.generation || !tcpPort_ ||
       network.local!=network.core || !usableAddress(network.local) ||
       !handshake::nonzero(pair_) || !handshake::nonzero(psk_)) {stop();return;}
    if(peer_ && !peer_->poll()) peer_.reset();
    if(listener_<0) {
      if(!retry_.due(now)) return;
      if(!listenOn(network.local)) {retry_.failure(now);return;}
      retry_.reset();
    }
    if(acceptSeen_ && now-lastAccept_<AcceptEveryUs) return;
    acceptSeen_=true;lastAccept_=now;
    int incoming=::accept(listener_,nullptr,nullptr);
    if(incoming<0) {
      // A full descriptor pool or transient accept error must not evict
      // the current authenticated peer. Bounded retry at next accept slot.
      return;
    }
    if(peer_) {
      closeFd(incoming);
      if(refused_!=UINT32_MAX) ++refused_;
      return;
    }
    if(!stream_.adopt(incoming)) return;
    peer_.emplace(stream_,clock_,crypto_,pair_,psk_,port_,ids_);
    if(!peer_->start()) peer_.reset();
  }
private:
  bool listenOn(uint32_t local) {
    int fd=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(fd<0) return false;
    const auto a=address(local,tcpPort_);
    const int one=1;
    if(!nonblocking(fd) || ::setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one))!=0 ||
       ::bind(fd,reinterpret_cast<const sockaddr*>(&a),sizeof(a))!=0 ||
       ::listen(fd,1)!=0) {closeFd(fd);return false;}
    listener_=fd;return true;
  }
  static constexpr uint64_t AcceptEveryUs=100000;
  const tr::ClockSource& clock_;const handshake::Crypto& crypto_;
  const handshake::PairId& pair_;const handshake::Secret& psk_;
  tr::CorePort& port_;handshake::LinkIds& ids_;const uint16_t tcpPort_;const Budget budget_;
  Network network_{};Retry retry_{};
  int listener_=-1;SocketStream stream_{};std::optional<tr::Peer> peer_;
  uint64_t lastNow_=0,lastAccept_=0;
  bool timeSeen_=false,clockFault_=false,acceptSeen_=false;
  unsigned refused_=0;
};

// Taster-only owner; persistent health belongs to the renderer and
// survives every TCP attempt. No Race Control/engine/store is required.
class TasterService {
public:
  TasterService(const tr::ClockSource& clock,const handshake::Crypto& crypto,
                const handshake::PairId& pair,const handshake::Secret& psk,
                health::Client& client,uint16_t tcpPort)
    :clock_(clock),crypto_(crypto),pair_(pair),psk_(psk),client_(client),tcpPort_(tcpPort){}
  ~TasterService(){stop();}
  TasterService(const TasterService&)=delete;
  TasterService& operator=(const TasterService&)=delete;
  bool connecting() const {return connecting_;}
  bool established() const {return peer_ && peer_->established();}
  Session session() const {return established()?peer_->session():Session{};}
  uint32_t attempts() const {return attempts_;}
  uint64_t retryWaitUs() const {return retry_.waitUs();}
  void stop(){
    peer_.reset();stream_.close();connecting_=false;
  }
  // Call before rendering/processing touch, even with no network activity.
  // Association and credentials are managed by a separate authorised
  // platform owner. Connect to a numeric Core AP address only, using the
  // chosen STA source address. A changed network epoch retires old input.
  void poll(const Network& network,bool commissioned) {
    const auto now=clock_.now();
    if(timeSeen_ && now<lastNow_) clockFault_=true;
    timeSeen_=true;lastNow_=now;
    if(!network.same(network_)) {stop();retry_.reset();network_=network;}
    if(clockFault_ || !commissioned || !network.ready || !network.generation ||
       !tcpPort_ || !usableAddress(network.local) || !usableAddress(network.core) ||
       !handshake::nonzero(pair_) || !handshake::nonzero(psk_)) {stop();return;}
    if(peer_) {
      if(!peer_->poll()) {failed(now);return;}
      // Only a fresh authenticated display, not TCP connect/handshake,
      // clears reconnect backoff. A flapping peer cannot cause a tight loop.
      if(client_.view(clock_.now()).state==health::State::Fresh) retry_.reset();
      return;
    }
    if(connecting_) {
      if(now-connectSince_>=ConnectUs) {failed(now);return;}
      const int fd=stream_.fd();
      if(fd<0 || fd>=FD_SETSIZE) {failed(now);return;}
      fd_set writable,errors;FD_ZERO(&writable);FD_ZERO(&errors);
      FD_SET(fd,&writable);FD_SET(fd,&errors);timeval zero{};
      const int ready=::select(fd+1,nullptr,&writable,&errors,&zero);
      if(ready<0) {if(!wouldBlock(errno)) failed(now);return;}
      if(!ready) return;
      int error=0;socklen_t length=sizeof(error);
      if(::getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&length)!=0 || error ||
         !FD_ISSET(fd,&writable) || FD_ISSET(fd,&errors)) {failed(now);return;}
      connecting_=false;
      if(!beginPeer()) failed(now);
      return;
    }
    if(!retry_.due(now)) return;
    if(attempts_!=UINT32_MAX) ++attempts_;
    const int fd=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(!stream_.adopt(fd)) {failed(now);return;}
    const auto source=address(network.local,0),dest=address(network.core,tcpPort_);
    if(::bind(fd,reinterpret_cast<const sockaddr*>(&source),sizeof(source))!=0) {
      failed(now);return;
    }
    const int rc=::connect(fd,reinterpret_cast<const sockaddr*>(&dest),sizeof(dest));
    if(rc==0) {if(!beginPeer()) failed(now);return;}
    const int error=errno;
    if(error!=EINPROGRESS && error!=EALREADY && !wouldBlock(error)) {failed(now);return;}
    connecting_=true;connectSince_=now;
  }
  // No implicit command queue/retry. Renderer supplies the existing
  // validated release/hold/confirmation and health's fresh touch.
  bool command(Action a,Mode m=Mode::None,uint16_t value=0,bool confirmed=false) {
    return peer_ && peer_->command(a,m,value,confirmed);
  }
private:
  bool beginPeer(){
    peer_.emplace(stream_,clock_,crypto_,pair_,psk_,client_);
    return peer_->start();
  }
  void failed(uint64_t now){stop();retry_.failure(now);}
  static constexpr uint64_t ConnectUs=2000000;
  const tr::ClockSource& clock_;const handshake::Crypto& crypto_;
  const handshake::PairId& pair_;const handshake::Secret& psk_;
  health::Client& client_;const uint16_t tcpPort_;
  Network network_{};Retry retry_{};SocketStream stream_{};std::optional<tr::Peer> peer_;
  uint64_t lastNow_=0,connectSince_=0;
  uint32_t attempts_=0;
  bool timeSeen_=false,clockFault_=false,connecting_=false;
};

} // namespace pp::taster_link::socket_runtime
