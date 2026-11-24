// Host checks for the shared network generation and local maintenance
// policy. No ESP header, real driver event or interface-binding evidence.
#include "pp/taster_link_platform.h"
#include <cassert>
#include <iostream>

namespace pf=pp::taster_link::platform;
namespace sr=pp::taster_link::socket_runtime;
int main(){
  pf::EventRevision events;
  pf::NetworkEpoch epoch;
  const uint32_t ip=htonl(INADDR_LOOPBACK),other=htonl(0x7f000002);
  sr::Network ap{true,0,ip,ip};
  ap.interfaceName={'a','p','0',0,0,0};
  auto sample=[&](sr::Network n){
    const auto r=events.value();return epoch.observe(n,r,r);
  };
  const auto first=sample(ap);
  assert(first.ready && first.generation && first.interfaceName==ap.interfaceName);
  assert(sample(ap).same(first));
  // AP/STA reinitialised between owner polls but DHCP reused the address.
  events.changed();events.changed();
  const auto second=sample(ap);
  assert(second.ready && second.generation>first.generation && !second.same(first));
  // Interface replacement with identical addresses also retires old epoch.
  ap.interfaceName={'a','p','1',0,0,0};
  const auto third=sample(ap);
  assert(third.generation>second.generation && !third.same(second));
  // Event during live observation: never publish a mixed state.
  const auto before=events.value();events.changed();
  assert(!epoch.observe(ap,before,events.value()).ready);
  const auto fourth=sample(ap);
  assert(fourth.ready && fourth.generation>third.generation);
  assert(!sample({}).ready);
  const auto fifth=sample(ap);
  assert(fifth.generation>fourth.generation);
  epoch.invalidate();
  const auto sixth=sample(ap);
  assert(sixth.ready && sixth.generation>fifth.generation);
  ap.local=other;ap.core=other;
  assert(sample(ap).generation>sixth.generation);
  assert(!epoch.observe(ap,UINT32_MAX,UINT32_MAX).ready && epoch.faulted());
  assert(!sample(ap).ready);
  // Host cannot pretend its loopback socket has verified ESP provenance.
  int fd=::socket(AF_INET,SOCK_STREAM,0);assert(fd>=0);
  assert(!sr::bindInterface(fd,ap));
  ap.interfaceName={};assert(sr::bindInterface(fd,ap));sr::closeFd(fd);
  assert(!sr::bindInterface(fd,ap));

  pf::LocalMaintenance gate;
  assert(!gate.locallyAuthorised() && !gate.safeLifecycle() && !gate.transportQuiescent());
  // Boot load can be quiescent without authorising a durable mutation.
  gate.update(false,true,false,true);
  assert(!gate.locallyAuthorised() && !gate.safeLifecycle() && gate.transportQuiescent());
  gate.update(true,false,true,true);assert(!gate.locallyAuthorised());
  gate.update(true,true,true,true);
  assert(gate.locallyAuthorised() && gate.safeLifecycle() && gate.transportQuiescent());
  gate.update(true,true,false,true);assert(!gate.safeLifecycle());
  gate.update(true,true,true,false);assert(!gate.transportQuiescent());
  std::cout<<"S3g2h network generation, interface boundary and maintenance gates PASS\n";
}
