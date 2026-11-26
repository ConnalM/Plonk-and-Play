#pragma once

#include "s4_production_role_guard.h"
#include "s4_role_startup.h"

namespace pp::s4::entrypoint {

inline constexpr const StartupPlan& kCorePlan = kCoreStartupPlan;
static_assert(validCoreStartupPlan());

}  // namespace pp::s4::entrypoint
