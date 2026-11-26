#pragma once

#include "s4_production_role_guard.h"
#include "s4_role_startup.h"

namespace pp::s4::entrypoint {

inline constexpr const StartupPlan& kTasterPlan = kTasterStartupPlan;
static_assert(validTasterStartupPlan());

}  // namespace pp::s4::entrypoint
