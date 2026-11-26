cmake_minimum_required(VERSION 3.16)

get_filename_component(PP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${PP_ROOT}/firmware/wp7_idf_arduino_dbuf/cmake/pp_s4_role_profiles.cmake")

function(require condition message_text)
  if(NOT ${condition})
    message(FATAL_ERROR "S4b role-profile check failed: ${message_text}")
  endif()
endfunction()

function(require_list_has list_name item)
  list(FIND ${list_name} "${item}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "S4b role-profile check failed: ${list_name} lacks ${item}")
  endif()
endfunction()

function(require_list_lacks list_name item)
  list(FIND ${list_name} "${item}" position)
  if(NOT position EQUAL -1)
    message(FATAL_ERROR "S4b role-profile check failed: ${list_name} contains ${item}")
  endif()
endfunction()

function(require_project_result role expected_text)
  # Script mode cannot configure ESP-IDF.  The S4b refusals must occur before
  # project() for the future roles; the integrated checks must instead reach
  # the historical Arduino prerequisite with its environment deliberately
  # absent.  Checking the exact diagnostic rules out unrelated failures.
  set(arguments)
  if(NOT role STREQUAL "DEFAULT")
    # Keep a multi-role attempt as one CMake -D argument rather than allowing
    # CMake's own list separator to turn it into CORE plus a stray TASTER.
    string(REPLACE ";" "\\;" role_argument "${role}")
    list(APPEND arguments "-DPP_S4_BUILD_ROLE=${role_argument}")
  endif()
  if(ARGC GREATER 2)
    # Keep the complete production-definition list in one argv element. The
    # child CMake process then parses normal semicolons as its list delimiter;
    # escaped/bracket-contained semicolons remain testable malformed values.
    string(REPLACE ";" "\\;" definitions_argument "${ARGV2}")
    list(APPEND arguments "-DPP_S4_PRODUCTION_ROLE_DEFINES=${definitions_argument}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" ${arguments}
            -P "${PP_ROOT}/firmware/wp7_idf_arduino_dbuf/CMakeLists.txt"
    RESULT_VARIABLE project_result
    OUTPUT_VARIABLE project_stdout
    ERROR_VARIABLE project_stderr)
  if(project_result EQUAL 0)
    message(FATAL_ERROR "S4b role-profile check failed: ${role} unexpectedly succeeded")
  endif()
  set(project_text "${project_stdout}${project_stderr}")
  string(FIND "${project_text}" "${expected_text}" expected_position)
  if(expected_position EQUAL -1)
    message(FATAL_ERROR
      "S4b role-profile check failed: ${role} did not report '${expected_text}':\n${project_text}")
  endif()
endfunction()

pp_s4_role_is_valid("INTEGRATED" integrated_valid)
pp_s4_role_is_valid("CORE" core_valid)
pp_s4_role_is_valid("TASTER" taster_valid)
pp_s4_role_is_valid("INVALID" invalid_valid)
require(integrated_valid "integrated role must be valid")
require(core_valid "Core role must be valid")
require(taster_valid "Taster role must be valid")
if(invalid_valid)
  message(FATAL_ERROR "S4b role-profile check failed: invalid role accepted")
endif()

pp_s4_select_role("INTEGRATED" integrated)
pp_s4_select_role("CORE" core)
pp_s4_select_role("TASTER" taster)
pp_s4_production_forbidden_definitions(production_forbidden)

require(integrated_LINKABLE "integrated profile must remain linkable")
pp_s4_validate_production_role_flags("INTEGRATED" "PP_PHASE8B_P1_TEST")
if(core_LINKABLE OR taster_LINKABLE)
  message(FATAL_ERROR "S4b role-profile check failed: future role scaffold became linkable")
endif()
if(NOT integrated_COMPONENT_DIRS STREQUAL "components/arduino;components/gfx;components/tamc")
  message(FATAL_ERROR "S4b role-profile check failed: integrated component order changed")
endif()
if(core_IDENTITY STREQUAL taster_IDENTITY OR
   core_IDENTITY STREQUAL integrated_IDENTITY OR
   taster_IDENTITY STREQUAL integrated_IDENTITY)
  message(FATAL_ERROR "S4b role-profile check failed: role identities are not unique")
endif()
require_list_has(integrated_COMPONENT_DIRS "components/gfx")
require_list_has(integrated_COMPONENT_DIRS "components/tamc")
require_list_lacks(core_COMPONENT_DIRS "components/gfx")
require_list_lacks(core_COMPONENT_DIRS "components/tamc")
require_list_lacks(core_REQUIRES "gfx")
require_list_lacks(core_REQUIRES "tamc")
require_list_lacks(core_REQUIRES "esp_lcd")
require_list_lacks(taster_REQUIRES "esp_http_server")
require_list_lacks(taster_REQUIRES "fatfs")
require_list_lacks(taster_REQUIRES "sdmmc")
require_list_has(core_REQUIRES "esp_http_server")
require_list_has(core_REQUIRES "fatfs")
require_list_has(core_REQUIRES "sdmmc")
require_list_has(taster_COMPONENT_DIRS "components/gfx")
require_list_has(taster_COMPONENT_DIRS "components/tamc")

list(LENGTH core_ASSETS core_asset_count)
if(NOT core_asset_count EQUAL 0)
  message(FATAL_ERROR "S4b role-profile check failed: Core has display assets")
endif()
require_list_has(taster_ASSETS "assets/wp7_approved/pp_header_logo.rgb565")
foreach(role core taster)
  foreach(flag PP_PHASE8B_P1_TEST PP_WP7_ACCEPTANCE_SIMULATION PP_SIMULATED_RACING_TEST PP_UPDATE_TEST_KEY PP_WP7_PHYSICAL_RACING_TEST)
    require_list_has(${role}_FORBIDDEN_DEFINES "${flag}")
  endforeach()
endforeach()

# Exercise the actual top-level project file, never ESP-IDF configuration.
# Emptying the inherited path makes both integrated cases prove they reach the
# unchanged legacy prerequisite rather than any ESP-IDF command.
set(ENV{PP_ARDUINO_COMPONENT_DIR} "")
require_project_result("CORE" "PP_S4_BUILD_ROLE=CORE is an S4b non-linkable scaffold")
require_project_result("TASTER" "PP_S4_BUILD_ROLE=TASTER is an S4b non-linkable scaffold")
require_project_result("INVALID" "PP_S4_BUILD_ROLE must be INTEGRATED, CORE or TASTER; got 'INVALID'")
require_project_result("CORE;TASTER" "PP_S4_BUILD_ROLE must be INTEGRATED, CORE or TASTER; got 'CORE;TASTER'")
require_project_result("DEFAULT" "PP_ARDUINO_COMPONENT_DIR must point to the Arduino-ESP32 component")
require_project_result("INTEGRATED" "PP_ARDUINO_COMPONENT_DIR must point to the Arduino-ESP32 component")
require_project_result("INTEGRATED" "PP_ARDUINO_COMPONENT_DIR must point to the Arduino-ESP32 component" "PP_PHASE8B_P1_TEST")
foreach(role CORE TASTER)
  foreach(flag IN LISTS production_forbidden)
    require_project_result("${role}" "S4 role flag rejected: ${role} ${flag}" "${flag}")
  endforeach()
  foreach(assigned PP_UPDATE_TEST_KEY=1 PP_WP7_PHYSICAL_RACING_TEST=0 PP_STAGE14C_ACCEPTANCE=reviewed)
    string(REGEX REPLACE "=.*$" "" assigned_name "${assigned}")
    require_project_result("${role}" "S4 role flag rejected: ${role} ${assigned_name}" "${assigned}")
  endforeach()
  foreach(malformed -DPP_UPDATE_TEST_KEY PP_UPDATE_TEST_KEY= 9INVALID PP_UPDATE_TEST_KEY=has=equals)
    require_project_result("${role}" "S4 role guard malformed definition '${malformed}'" "${malformed}")
  endforeach()
  set(escaped_semicolon "GOOD=x\\;PP_UPDATE_TEST_KEY")
  set(bracket_semicolon [=[GOOD=[x;PP_UPDATE_TEST_KEY]=])
  foreach(malformed "${escaped_semicolon}" "${bracket_semicolon}" ";GOOD=one" "GOOD=one;" "GOOD=one;;OTHER=two")
    require_project_result("${role}" "S4 role guard malformed" "${malformed}")
  endforeach()
  require_project_result("${role}" "PP_S4_BUILD_ROLE=${role} is an S4b non-linkable scaffold" "GOOD=one;OTHER=two")
  require_project_result("${role}" "S4 role flag rejected: ${role} PP_UPDATE_TEST_KEY" "GOOD=one;PP_UPDATE_TEST_KEY=0")
endforeach()

message(STATUS "S4b independent role build-profile contract PASS")
