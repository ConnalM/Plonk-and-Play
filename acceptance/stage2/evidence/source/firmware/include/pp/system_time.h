#pragma once
#include <stdint.h>

namespace pp {
// Shared controller time service: monotonic microseconds within one boot.
// Not wall-clock time; no dependency on diagnostics or transport.
uint64_t systemTime();
}
