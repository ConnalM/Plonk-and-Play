#include "pp/s4_taster_entrypoint_boundary.h"

// Compile-only future Taster entry-point boundary. No app_main/setup/loop or
// display/touch/link activation is supplied until independent target facts exist.
static_assert(pp::s4::entrypoint::kTasterPlan.role == pp::s4::RuntimeRole::Taster);
static_assert(pp::s4::kProductionRole != pp::s4::ProductionRole::Core,
              "Taster boundary cannot be selected by a Core production role");
