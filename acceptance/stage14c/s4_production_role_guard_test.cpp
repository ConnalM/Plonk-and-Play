#include "pp/s4_production_role_guard.h"

static_assert(pp::s4::kProductionRole == pp::s4::ProductionRole::Integrated ||
              pp::s4::kProductionRole == pp::s4::ProductionRole::Core ||
              pp::s4::kProductionRole == pp::s4::ProductionRole::Taster);

#if defined(PP_S4_GUARD_EXPECT_CORE)
static_assert(pp::s4::kProductionRole == pp::s4::ProductionRole::Core);
#endif
#if defined(PP_S4_GUARD_EXPECT_TASTER)
static_assert(pp::s4::kProductionRole == pp::s4::ProductionRole::Taster);
#endif
#if defined(PP_S4_GUARD_EXPECT_INTEGRATED)
static_assert(pp::s4::kProductionRole == pp::s4::ProductionRole::Integrated);
#endif

int main() { return 0; }
