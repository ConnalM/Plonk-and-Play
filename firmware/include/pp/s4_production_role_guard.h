#pragma once

// Enforce the minimum production-role diagnostic boundary at compile time.
// This header is inert for the accepted integrated composition. Future Core or
// Taster entry points must include it before any application service headers.

#if defined(PP_S4_PRODUCTION_CORE) && defined(PP_S4_PRODUCTION_TASTER)
#error "S4 production build must select exactly one role"
#endif

#if defined(PP_S4_PRODUCTION_CORE) || defined(PP_S4_PRODUCTION_TASTER)
#if defined(PP_PHASE8B_P1_TEST)
#error "S4 production role rejects PP_PHASE8B_P1_TEST"
#endif
#if defined(PP_WP7_ACCEPTANCE_SIMULATION)
#error "S4 production role rejects PP_WP7_ACCEPTANCE_SIMULATION"
#endif
#if defined(PP_SIMULATED_RACING_TEST)
#error "S4 production role rejects PP_SIMULATED_RACING_TEST"
#endif
#if defined(PP_UPDATE_TEST_KEY)
#error "S4 production role rejects PP_UPDATE_TEST_KEY"
#endif
#if defined(PP_WP7_PHYSICAL_RACING_TEST)
#error "S4 production role rejects PP_WP7_PHYSICAL_RACING_TEST"
#endif

// Audited additional fixture, acceptance and diagnostic entry paths. These
// are intentionally unavailable to either future production role.
#if defined(PP_WP6_ROLLBACK_ACCEPTANCE) || defined(PP_ACCEPTANCE) || \
    defined(PP_VERIFY) || defined(PP_WP8_NVS_DIAGNOSTIC) || \
    defined(PP_STAGE8A_TASTER_WOKWI) || defined(PP_WP7_TOUCH_TEST) || \
    defined(PP_WP7_READY_TEST) || defined(PP_STAGE15_PHASE8_SD) || \
    defined(PP_STAGE2_ACCEPTANCE) || defined(PP_STAGE3_ACCEPTANCE) || \
    defined(PP_STAGE4_ACCEPTANCE) || defined(PP_STAGE5_ACCEPTANCE) || \
    defined(PP_STAGE6_ACCEPTANCE) || defined(PP_STAGE6_DEMO) || \
    defined(PP_STAGE7_ACCEPTANCE) || defined(PP_STAGE7_DEMO) || \
    defined(PP_STAGE8_ACCEPTANCE) || defined(PP_STAGE9_ACCEPTANCE) || \
    defined(PP_STAGE9_DEMO) || defined(PP_STAGE10_ACCEPTANCE) || \
    defined(PP_STAGE10_DEMO) || defined(PP_STAGE11_ACCEPTANCE) || \
    defined(PP_STAGE11_DEMO) || defined(PP_STAGE12_ACCEPTANCE) || \
    defined(PP_STAGE12_DEMO) || defined(PP_STAGE13_ACCEPTANCE) || \
    defined(PP_STAGE13_DEMO) || defined(PP_STAGE14A_ACCEPTANCE) || \
    defined(PP_STAGE14A_DEMO) || defined(PP_STAGE14B_ACCEPTANCE) || \
    defined(PP_STAGE14B_DEMO) || defined(PP_STAGE14C_ACCEPTANCE) || \
    defined(PP_STAGE14C_DEMO)
#error "S4 production role rejects an acceptance, demo or diagnostic fixture flag"
#endif
#endif

namespace pp::s4 {

enum class ProductionRole : unsigned char { Integrated, Core, Taster };

#if defined(PP_S4_PRODUCTION_CORE)
inline constexpr ProductionRole kProductionRole = ProductionRole::Core;
#elif defined(PP_S4_PRODUCTION_TASTER)
inline constexpr ProductionRole kProductionRole = ProductionRole::Taster;
#else
inline constexpr ProductionRole kProductionRole = ProductionRole::Integrated;
#endif

}  // namespace pp::s4
