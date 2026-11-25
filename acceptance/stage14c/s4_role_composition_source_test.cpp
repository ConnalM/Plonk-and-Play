#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

static std::string source(const char* path) {
  std::ifstream in(path);assert(in.good());
  std::ostringstream text;text << in.rdbuf();return text.str();
}
static bool has(const std::string& text,const char* token) {
  return text.find(token)!=std::string::npos;
}

int main() {
  const auto core=source("firmware/include/pp/taster_link_startup_core_esp.h");
  const auto taster=source("firmware/include/pp/taster_link_startup_taster_esp.h");
  const auto corePlatform=source("firmware/include/pp/taster_link_platform_core_esp.h");
  const auto tasterPlatform=source("firmware/include/pp/taster_link_platform_taster_esp.h");
  const auto combined=source("firmware/src/wp8_role_compile_probe.cpp");
  const auto coreProbe=source("firmware/src/wp8_core_role_compile_probe.cpp");
  const auto tasterProbe=source("firmware/src/wp8_taster_role_compile_probe.cpp");
  const auto cmake=source("firmware/wp7_idf_arduino_dbuf/main/CMakeLists.txt");
  const auto main=source("firmware/src/main.cpp");

  // These are source-boundary checks, not ESP target compiler evidence.
  assert(has(core,"taster_link_platform_core_esp.h"));
  assert(has(core,"taster_link_core_port.h"));
  assert(has(core,"taster_link_pairing_esp_store.h"));
  assert(has(core,"CoreRuntime"));
  assert(!has(core,"taster_link_platform_taster_esp.h"));
  assert(!has(core,"Arduino_GFX"));
  assert(!has(core,"TAMC"));
  assert(has(taster,"taster_link_platform_taster_esp.h"));
  assert(has(taster,"TasterRuntime"));
  assert(!has(taster,"taster_link_startup_core_esp.h"));
  assert(!has(taster,"taster_link_core_port.h"));
  assert(!has(taster,"RaceControlModule"));
  assert(!has(taster,"RaceControlPort"));
  assert(!has(tasterPlatform,"race_control.h"));
  assert(!has(tasterPlatform,"Arduino_GFX"));
  assert(!has(tasterPlatform,"TAMC"));
  assert(!has(corePlatform,"Arduino_GFX"));
  assert(!has(corePlatform,"TAMC"));
  assert(!has(corePlatform,"taster_link_discovery_esp.h"));
  assert(has(source("firmware/include/pp/taster_link_startup_common_esp.h"),
             "taster_link_pairing.h"));
  assert(has(combined,"INADDR_NONE"));
  assert(has(combined,"void core("));
  assert(has(combined,"void taster("));
  assert(has(coreProbe,"taster_link_startup_core_esp.h"));
  assert(has(coreProbe,"INADDR_NONE"));
  assert(!has(coreProbe,"taster_link_startup_taster_esp.h"));
  assert(has(tasterProbe,"taster_link_startup_taster_esp.h"));
  assert(has(tasterProbe,"INADDR_NONE"));
  assert(!has(tasterProbe,"RaceControlModule"));
  assert(!has(tasterProbe,"taster_link_startup_core_esp.h"));
  assert(has(cmake,"PP_WP8_CORE_ROLE_COMPILE_PROBE"));
  assert(has(cmake,"PP_WP8_TASTER_ROLE_COMPILE_PROBE"));
  assert(!has(main,"wp8_core_role_compile_probe"));
  assert(!has(main,"wp8_taster_role_compile_probe"));
}
