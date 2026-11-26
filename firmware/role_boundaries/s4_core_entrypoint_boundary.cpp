#include "pp/s4_core_entrypoint_boundary.h"

// Compile-only future Core entry-point boundary. No app_main/setup/loop or
// hardware/service activation is supplied until independent target facts exist.
static_assert(pp::s4::entrypoint::kCorePlan.role == pp::s4::RuntimeRole::Core);
static_assert(pp::s4::kProductionRole != pp::s4::ProductionRole::Taster,
              "Core boundary cannot be selected by a Taster production role");
