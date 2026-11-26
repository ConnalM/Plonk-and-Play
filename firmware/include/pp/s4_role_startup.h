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
  // A protected store is required by the optional authenticated Taster link,
  // but its availability must not block Core sporting/Browser startup.
  Service gatedLinkPrerequisite;
};

inline constexpr StartupPlan kCoreStartupPlan{
    RuntimeRole::Core,
    {StartupStep::Configuration, StartupStep::PersistentOwnership,
     StartupStep::SportingAuthority, StartupStep::RemoteControl,
     StartupStep::CustomerUpdate},
    {Service::CommittedPreferences, Service::SharedStorage,
     Service::DetectorInput, Service::RaceAuthority, Service::SmuglandHttp,
     Service::CustomerUpdateAuthority, Service::ProtectedPairingStore,
     Service::TasterLinkListener},
    8u,
    Service::ProtectedPairingStore,
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
    Service::ProtectedPairingStore,
};

constexpr bool planUsesOnlyOwnedServices(const StartupPlan& plan,
                                         const RoleManifest& manifest) {
  for (size_t index = 0; index < plan.requiredCount; ++index) {
    if (!instantiates(manifest, plan.requiredServices[index])) return false;
  }
  return true;
}

constexpr uint32_t requiredServiceMask(const StartupPlan& plan) {
  uint32_t mask = 0;
  for (size_t index = 0; index < plan.requiredCount; ++index) {
    mask |= serviceMask(plan.requiredServices[index]);
  }
  return mask;
}

constexpr bool hasUniqueRequiredServices(const StartupPlan& plan) {
  if (plan.requiredCount > plan.requiredServices.size()) return false;
  for (size_t left = 0; left < plan.requiredCount; ++left) {
    for (size_t right = left + 1; right < plan.requiredCount; ++right) {
      if (plan.requiredServices[left] == plan.requiredServices[right]) return false;
    }
  }
  return true;
}

constexpr bool planMatchesManifest(const StartupPlan& plan,
                                   const RoleManifest& manifest) {
  return plan.role == manifest.role && hasUniqueRequiredServices(plan) &&
         planUsesOnlyOwnedServices(plan, manifest) &&
         instantiates(manifest, plan.gatedLinkPrerequisite) &&
         requiredServiceMask(plan) == manifest.instantiatedServices;
}

constexpr bool validCoreStartupPlan() {
  return kCoreStartupPlan.role == RuntimeRole::Core &&
         kCoreStartupPlan.steps[0] == StartupStep::Configuration &&
         kCoreStartupPlan.steps[1] == StartupStep::PersistentOwnership &&
         kCoreStartupPlan.steps[2] == StartupStep::SportingAuthority &&
         kCoreStartupPlan.steps[3] == StartupStep::RemoteControl &&
         kCoreStartupPlan.steps[4] == StartupStep::CustomerUpdate &&
         planMatchesManifest(kCoreStartupPlan, kCoreManifest) &&
         kCoreStartupPlan.gatedLinkPrerequisite == Service::ProtectedPairingStore &&
         !instantiates(kCoreManifest, Service::LocalDisplay) &&
         !instantiates(kCoreManifest, Service::LocalTouch);
}

constexpr bool validTasterStartupPlan() {
  return kTasterStartupPlan.role == RuntimeRole::Taster &&
         kTasterStartupPlan.steps[0] == StartupStep::Configuration &&
         kTasterStartupPlan.steps[1] == StartupStep::PersistentOwnership &&
         kTasterStartupPlan.steps[2] == StartupStep::RemoteControl &&
         kTasterStartupPlan.steps[3] == StartupStep::LocalPresentation &&
         kTasterStartupPlan.steps[4] == StartupStep::GatedInteraction &&
         planMatchesManifest(kTasterStartupPlan, kTasterManifest) &&
         kTasterStartupPlan.gatedLinkPrerequisite == Service::ProtectedPairingStore &&
         !instantiates(kTasterManifest, Service::RaceAuthority) &&
         !instantiates(kTasterManifest, Service::SharedStorage) &&
         !instantiates(kTasterManifest, Service::SmuglandHttp);
}

static_assert(validCoreStartupPlan(), "S4 Core startup ownership changed");
static_assert(validTasterStartupPlan(), "S4 Taster startup ownership changed");

}  // namespace pp::s4
