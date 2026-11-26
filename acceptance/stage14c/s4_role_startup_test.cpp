#include "pp/s4_role_startup.h"

#include <cassert>
#include <iostream>

namespace s4 = pp::s4;

int main() {
  static_assert(s4::validCoreStartupPlan());
  static_assert(s4::validTasterStartupPlan());
  assert(s4::kCoreStartupPlan.steps[0] == s4::StartupStep::Configuration);
  assert(s4::kCoreStartupPlan.steps[1] == s4::StartupStep::PersistentOwnership);
  assert(s4::kCoreStartupPlan.steps[2] == s4::StartupStep::SportingAuthority);
  assert(s4::kCoreStartupPlan.steps[3] == s4::StartupStep::RemoteControl);
  assert(s4::kCoreStartupPlan.steps[4] == s4::StartupStep::CustomerUpdate);
  assert(s4::planMatchesManifest(s4::kCoreStartupPlan, s4::kCoreManifest));
  assert(s4::requiredServiceMask(s4::kCoreStartupPlan) ==
         s4::kCoreManifest.instantiatedServices);
  assert(s4::hasUniqueRequiredServices(s4::kCoreStartupPlan));
  assert(s4::kCoreStartupPlan.gatedLinkPrerequisite ==
         s4::Service::ProtectedPairingStore);
  assert(!s4::planMatchesManifest(s4::kCoreStartupPlan, s4::kTasterManifest));
  assert(s4::kTasterStartupPlan.steps[0] == s4::StartupStep::Configuration);
  assert(s4::kTasterStartupPlan.steps[1] == s4::StartupStep::PersistentOwnership);
  assert(s4::kTasterStartupPlan.steps[2] == s4::StartupStep::RemoteControl);
  assert(s4::kTasterStartupPlan.steps[3] == s4::StartupStep::LocalPresentation);
  assert(s4::kTasterStartupPlan.steps[4] == s4::StartupStep::GatedInteraction);
  assert(s4::planMatchesManifest(s4::kTasterStartupPlan, s4::kTasterManifest));
  assert(s4::requiredServiceMask(s4::kTasterStartupPlan) ==
         s4::kTasterManifest.instantiatedServices);
  assert(s4::hasUniqueRequiredServices(s4::kTasterStartupPlan));
  assert(s4::kTasterStartupPlan.gatedLinkPrerequisite ==
         s4::Service::ProtectedPairingStore);
  assert(!s4::planMatchesManifest(s4::kTasterStartupPlan, s4::kCoreManifest));
  assert(!s4::instantiates(s4::kTasterManifest, s4::Service::RaceAuthority));
  assert(!s4::instantiates(s4::kTasterManifest, s4::Service::SharedStorage));
  assert(!s4::instantiates(s4::kTasterManifest, s4::Service::SmuglandHttp));
  std::cout << "S4 role startup ownership host contract PASS\n";
}
