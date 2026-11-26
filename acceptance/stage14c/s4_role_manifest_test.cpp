#include "pp/s4_role_manifest.h"
#if !defined(_WIN32)
#include "pp/taster_link_socket_runtime.h"
#endif

#include <cassert>
#include <iostream>

namespace s4 = pp::s4;

int main() {
  static_assert(s4::validArchitecture(s4::kCoreSocketBudget));
  static_assert(s4::kCoreSocketBudget.httpClients == 7u);
  static_assert(s4::kCoreSocketBudget.sseClients == 4u);
  static_assert(s4::kCoreSocketBudget.architecturalFloor == 12u);
  static_assert(s4::kCoreSocketBudget.runtimeAdmissionFloor == 13u);

  // Integrated remains the declared legacy composition for the accepted
  // firmware; S4b neither starts nor changes it.
  assert(s4::instantiates(s4::kIntegratedManifest, s4::Service::RaceAuthority));
  assert(s4::instantiates(s4::kIntegratedManifest, s4::Service::CommittedPreferences));
  assert(s4::instantiates(s4::kIntegratedManifest, s4::Service::LocalPresentationPrefs));
  assert(s4::instantiates(s4::kIntegratedManifest, s4::Service::LocalDisplay));
  assert(!s4::instantiates(s4::kIntegratedManifest, s4::Service::TasterLinkListener));
  assert(!s4::instantiates(s4::kIntegratedManifest, s4::Service::TasterLinkClient));
  assert(!s4::instantiates(s4::kIntegratedManifest, s4::Service::GatedRemoteInteraction));
  assert(!s4::instantiates(s4::kIntegratedManifest,
                           s4::Service::ProtectedPairingStore));
  assert(!s4::instantiates(s4::kIntegratedManifest, s4::Service::TasterLocatorStore));

  // Core is authoritative but has no presentation hardware service.
  assert(s4::instantiates(s4::kCoreManifest, s4::Service::DetectorInput));
  assert(s4::instantiates(s4::kCoreManifest, s4::Service::RaceAuthority));
  assert(s4::instantiates(s4::kCoreManifest, s4::Service::SharedStorage));
  assert(s4::instantiates(s4::kCoreManifest, s4::Service::SmuglandHttp));
  assert(s4::instantiates(s4::kCoreManifest, s4::Service::CommittedPreferences));
  assert(s4::instantiates(s4::kCoreManifest, s4::Service::ProtectedPairingStore));
  assert(!s4::instantiates(s4::kCoreManifest, s4::Service::LocalDisplay));
  assert(!s4::instantiates(s4::kCoreManifest, s4::Service::LocalTouch));

  // Taster is presentation plus gated client link, not an authority host.
  assert(s4::instantiates(s4::kTasterManifest, s4::Service::TasterLinkClient));
  assert(s4::instantiates(s4::kTasterManifest, s4::Service::LocalDisplay));
  assert(s4::instantiates(s4::kTasterManifest, s4::Service::LocalTouch));
  assert(s4::instantiates(s4::kTasterManifest, s4::Service::LocalPresentationPrefs));
  assert(s4::instantiates(s4::kTasterManifest, s4::Service::GatedRemoteInteraction));
  assert(s4::instantiates(s4::kTasterManifest, s4::Service::ProtectedPairingStore));
  assert(s4::instantiates(s4::kTasterManifest, s4::Service::TasterLocatorStore));
  assert(!s4::instantiates(s4::kTasterManifest, s4::Service::TasterLinkListener));
  assert(!s4::instantiates(s4::kTasterManifest, s4::Service::DetectorInput));
  assert(!s4::instantiates(s4::kTasterManifest, s4::Service::RaceAuthority));
  assert(!s4::instantiates(s4::kTasterManifest, s4::Service::SharedStorage));
  assert(!s4::instantiates(s4::kTasterManifest, s4::Service::SmuglandHttp));
  assert(!s4::instantiates(s4::kTasterManifest,
                           s4::Service::CustomerUpdateAuthority));

  // Match the existing link owner's actual admission contract on portable
  // POSIX CI: 10 existing sockets and three temporary/link descriptors.
  // This is not a measurement. MinGW lacks the POSIX socket headers used by
  // the existing host adapter, so the platform-independent manifest checks
  // above remain the local Windows contract.
#if !defined(_WIN32)
  assert((!pp::taster_link::socket_runtime::Budget{12u, 10u}.permits()));
  assert((pp::taster_link::socket_runtime::Budget{13u, 10u}.permits()));
#endif
  assert(!s4::permitsRuntimeAdmission(s4::kCoreSocketBudget, 12u));
  assert(s4::permitsRuntimeAdmission(s4::kCoreSocketBudget, 13u));

  std::cout << "S4b role-manifest host contract PASS\n";
}
