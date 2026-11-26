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
  assert(s4::kTasterStartupPlan.steps[0] == s4::StartupStep::Configuration);
  assert(s4::kTasterStartupPlan.steps[3] == s4::StartupStep::LocalPresentation);
  assert(s4::kTasterStartupPlan.steps[4] == s4::StartupStep::GatedInteraction);
  assert(!s4::instantiates(s4::kTasterManifest, s4::Service::RaceAuthority));
  assert(!s4::instantiates(s4::kTasterManifest, s4::Service::SharedStorage));
  assert(!s4::instantiates(s4::kTasterManifest, s4::Service::SmuglandHttp));
  std::cout << "S4 role startup ownership host contract PASS\n";
}
