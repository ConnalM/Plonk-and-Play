#include "pp/s4_role_manifest.h"

#include <cassert>
#include <iostream>

namespace s4 = pp::s4;

int main() {
  static_assert(s4::valid(s4::kCoreSocketBudget));
  static_assert(s4::kCoreSocketBudget.httpClients == 7u);
  static_assert(s4::kCoreSocketBudget.sseClients == 4u);
  static_assert(s4::kCoreSocketBudget.minimumTotalSockets == 12u);

  // Integrated remains the declared legacy composition for the accepted
  // firmware; S4b neither starts nor changes it.
  assert(s4::instantiates(s4::kIntegratedManifest, s4::Service::RaceAuthority));
  assert(s4::instantiates(s4::kIntegratedManifest, s4::Service::LocalDisplay));

  // Core is authoritative but has no presentation hardware service.
  assert(s4::instantiates(s4::kCoreManifest, s4::Service::DetectorInput));
  assert(s4::instantiates(s4::kCoreManifest, s4::Service::RaceAuthority));
  assert(s4::instantiates(s4::kCoreManifest, s4::Service::SharedStorage));
  assert(s4::instantiates(s4::kCoreManifest, s4::Service::SmuglandHttp));
  assert(!s4::instantiates(s4::kCoreManifest, s4::Service::LocalDisplay));
  assert(!s4::instantiates(s4::kCoreManifest, s4::Service::LocalTouch));

  // Taster is presentation plus gated client link, not an authority host.
  assert(s4::instantiates(s4::kTasterManifest, s4::Service::TasterLinkClient));
  assert(s4::instantiates(s4::kTasterManifest, s4::Service::LocalDisplay));
  assert(s4::instantiates(s4::kTasterManifest, s4::Service::LocalTouch));
  assert(!s4::instantiates(s4::kTasterManifest, s4::Service::DetectorInput));
  assert(!s4::instantiates(s4::kTasterManifest, s4::Service::RaceAuthority));
  assert(!s4::instantiates(s4::kTasterManifest, s4::Service::SharedStorage));
  assert(!s4::instantiates(s4::kTasterManifest, s4::Service::SmuglandHttp));
  assert(!s4::instantiates(s4::kTasterManifest, s4::Service::FirmwareUpdate));

  std::cout << "S4b role-manifest host contract PASS\n";
}
