#ifdef PP_ACCEPTANCE
#include "pp/system_time.h"
// Independent translation unit: verifies the production shared service is linkable.
uint64_t acceptanceReadSharedTime() { return pp::systemTime(); }
#endif
