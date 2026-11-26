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
  const auto common=source("firmware/include/pp/taster_link_startup_common_esp.h");
  const auto corePlatform=source("firmware/include/pp/taster_link_platform_core_esp.h");
  const auto tasterPlatform=source("firmware/include/pp/taster_link_platform_taster_esp.h");
  const auto combined=source("firmware/src/wp8_role_compile_probe.cpp");
  const auto coreProbe=source("firmware/src/wp8_core_role_compile_probe.cpp");
  const auto tasterProbe=source("firmware/src/wp8_taster_role_compile_probe.cpp");
  const auto cmake=source("firmware/wp7_idf_arduino_dbuf/main/CMakeLists.txt");
  const auto projectCmake=source("firmware/wp7_idf_arduino_dbuf/CMakeLists.txt");
  const auto profiles=source("firmware/wp7_idf_arduino_dbuf/cmake/pp_s4_role_profiles.cmake");
  const auto roleContract=source("firmware/include/pp/taster_link_role_contract.h");
  const auto productionGuard=source("firmware/include/pp/s4_production_role_guard.h");
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
  const auto lwip=corePlatform.find("#include <lwip/inet.h>");
  const auto push=corePlatform.find("#pragma push_macro(\"INADDR_NONE\")");
  const auto undef=corePlatform.find("#undef INADDR_NONE");
  const auto arduino=corePlatform.find("#include <Arduino.h>");
  const auto wifi=corePlatform.find("#include <WiFi.h>");
  const auto pop=corePlatform.find("#pragma pop_macro(\"INADDR_NONE\")");
  const auto platform=corePlatform.find("#include \"taster_link_platform.h\"");
  const auto lifecycle=corePlatform.find("#include \"taster_link_lifecycle.h\"");
  const auto discovery=corePlatform.find("#include \"taster_link_discovery.h\"");
  const auto race=corePlatform.find("#include \"race_control.h\"");
  assert(lwip<push && push<undef && undef<arduino && arduino<wifi && wifi<pop);
  assert(pop<platform && platform<lifecycle && lifecycle<discovery && discovery<race);
  assert(has(common,"taster_link_pairing.h"));
  assert(has(common,"taster_link_role_contract.h"));
  assert(has(common,"s4_production_role_guard.h"));
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
  assert(has(projectCmake,"PP_S4_BUILD_ROLE \"INTEGRATED\""));
  assert(has(projectCmake,"PP_S4_PROFILE_LINKABLE"));
  assert(has(projectCmake,"PP_S4_PRODUCTION_ROLE_DEFINES"));
  assert(!has(projectCmake,"PP_S4_FORCE_NINJA_RESPONSE_FILES"));
  assert(!has(projectCmake,"CMAKE_NINJA_FORCE_RESPONSE_FILE"));
  assert(has(profiles,"core-scaffold-not-linkable"));
  assert(has(profiles,"taster-scaffold-not-linkable"));
  assert(has(profiles,"PP_WP7_ACCEPTANCE_SIMULATION"));
  assert(has(profiles,"PP_SIMULATED_RACING_TEST"));
  assert(has(profiles,"PP_PHASE8B_P1_TEST"));
  assert(has(profiles,"PP_UPDATE_TEST_KEY"));
  assert(has(profiles,"PP_WP7_PHYSICAL_RACING_TEST"));
  assert(has(profiles,"pp_s4_validate_production_role_flags"));
  assert(has(profiles,"pp_s4_production_forbidden_definitions"));
  assert(has(roleContract,"struct Approval"));
  assert(has(roleContract,"class EventSlot"));
  assert(!has(roleContract,"#include <Arduino"));
  assert(!has(roleContract,"RaceControl"));
  assert(has(productionGuard,"PP_S4_PRODUCTION_CORE"));
  assert(has(productionGuard,"PP_S4_PRODUCTION_TASTER"));
  assert(has(productionGuard,"PP_STAGE14C_ACCEPTANCE"));
  for(const char* flag : {"PP_PHASE8B_P1_TEST", "PP_WP7_ACCEPTANCE_SIMULATION",
                          "PP_SIMULATED_RACING_TEST", "PP_UPDATE_TEST_KEY",
                          "PP_WP7_PHYSICAL_RACING_TEST", "PP_WP6_ROLLBACK_ACCEPTANCE",
                          "PP_ACCEPTANCE", "PP_VERIFY", "PP_WP8_NVS_DIAGNOSTIC",
                          "PP_STAGE8A_TASTER_WOKWI", "PP_WP7_TOUCH_TEST",
                          "PP_WP7_READY_TEST", "PP_STAGE15_PHASE8_SD",
                          "PP_STAGE2_ACCEPTANCE", "PP_STAGE3_ACCEPTANCE",
                          "PP_STAGE4_ACCEPTANCE", "PP_STAGE5_ACCEPTANCE",
                          "PP_STAGE6_ACCEPTANCE", "PP_STAGE6_DEMO",
                          "PP_STAGE7_ACCEPTANCE", "PP_STAGE7_DEMO",
                          "PP_STAGE8_ACCEPTANCE", "PP_STAGE9_ACCEPTANCE",
                          "PP_STAGE9_DEMO", "PP_STAGE10_ACCEPTANCE",
                          "PP_STAGE10_DEMO", "PP_STAGE11_ACCEPTANCE",
                          "PP_STAGE11_DEMO", "PP_STAGE12_ACCEPTANCE",
                          "PP_STAGE12_DEMO", "PP_STAGE13_ACCEPTANCE",
                          "PP_STAGE13_DEMO", "PP_STAGE14A_ACCEPTANCE",
                          "PP_STAGE14A_DEMO", "PP_STAGE14B_ACCEPTANCE",
                          "PP_STAGE14B_DEMO", "PP_STAGE14C_ACCEPTANCE",
                          "PP_STAGE14C_DEMO"}) {
    assert(has(profiles, flag));
    assert(has(productionGuard, flag));
  }
  assert(has(profiles,"components/gfx"));
  assert(has(profiles,"components/tamc"));
  assert(has(profiles,"esp_http_server"));
  assert(has(profiles,"sdmmc"));
  assert(!has(main,"wp8_core_role_compile_probe"));
  assert(!has(main,"wp8_taster_role_compile_probe"));
}
