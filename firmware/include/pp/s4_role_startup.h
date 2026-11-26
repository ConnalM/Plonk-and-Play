#pragma once

// Board-neutral startup ownership plans for future independent entry points.
// They contain no device setup, service implementation or application main.

#include "s4_role_manifest.h"

#include <array>
#include <cstddef>

namespace pp::s4 {

enum class StartupStep : uint8_t {
  Configuration,
  PersistentOwnership,
  SportingAuthority,
  RemoteControl,
  CustomerUpdate,
  LocalPresentation,
  GatedInteraction,
};

struct StartupPlan {
  RuntimeRole role;
  std::array<StartupStep, 5> steps;
  std::array<Service, 8> requiredServices;
  size_t requiredCount;
};

inline constexpr StartupPlan kCoreStartupPlan{
    RuntimeRole::Core,
    {StartupStep::Configuration, StartupStep::PersistentOwnership,
     StartupStep::SportingAuthority, StartupStep::RemoteControl,
     StartupStep::CustomerUpdate},
    {Service::CommittedPreferences, Service::SharedStorage,
     Service::DetectorInput, Service::RaceAuthority, Service::SmuglandHttp,
     Service::CustomerUpdateAuthority, Service::TasterLinkListener},
    7u,
};

inline constexpr StartupPlan kTasterStartupPlan{
    RuntimeRole::Taster,
    {StartupStep::Configuration, StartupStep::PersistentOwnership,
     StartupStep::RemoteControl, StartupStep::LocalPresentation,
     StartupStep::GatedInteraction},
    {Service::LocalPresentationPrefs, Service::ProtectedPairingStore,
     Service::TasterLocatorStore, Service::TasterLinkClient,
     Service::GatedRemoteInteraction, Service::LocalDisplay,
     Service::LocalTouch},
    7u,
};

constexpr bool planUsesOnlyOwnedServices(const StartupPlan& plan,
                                         const RoleManifest& manifest) {
  for (size_t index = 0; index < plan.requiredCount; ++index) {
    if (!instantiates(manifest, plan.requiredServices[index])) return false;
  }
  return true;
}

constexpr bool validCoreStartupPlan() {
  return kCoreStartupPlan.role == RuntimeRole::Core &&
         kCoreStartupPlan.steps[0] == StartupStep::Configuration &&
         kCoreStartupPlan.steps[1] == StartupStep::PersistentOwnership &&
         kCoreStartupPlan.steps[2] == StartupStep::SportingAuthority &&
         kCoreStartupPlan.steps[3] == StartupStep::RemoteControl &&
         kCoreStartupPlan.steps[4] == StartupStep::CustomerUpdate &&
         planUsesOnlyOwnedServices(kCoreStartupPlan, kCoreManifest) &&
         !instantiates(kCoreManifest, Service::LocalDisplay) &&
         !instantiates(kCoreManifest, Service::LocalTouch);
}

constexpr bool validTasterStartupPlan() {
  return kTasterStartupPlan.role == RuntimeRole::Taster &&
         kTasterStartupPlan.steps[0] == StartupStep::Configuration &&
         kTasterStartupPlan.steps[3] == StartupStep::LocalPresentation &&
         planUsesOnlyOwnedServices(kTasterStartupPlan, kTasterManifest) &&
         !instantiates(kTasterManifest, Service::RaceAuthority) &&
         !instantiates(kTasterManifest, Service::SharedStorage) &&
         !instantiates(kTasterManifest, Service::SmuglandHttp);
}

static_assert(validCoreStartupPlan(), "S4 Core startup ownership changed");
static_assert(validTasterStartupPlan(), "S4 Taster startup ownership changed");

}  // namespace pp::s4
