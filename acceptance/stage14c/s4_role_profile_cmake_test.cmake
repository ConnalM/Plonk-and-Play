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

require(integrated_LINKABLE "integrated profile must remain linkable")
if(core_LINKABLE OR taster_LINKABLE)
  message(FATAL_ERROR "S4b role-profile check failed: future role scaffold became linkable")
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
  foreach(flag PP_WP7_ACCEPTANCE_SIMULATION PP_SIMULATED_RACING_TEST PP_UPDATE_TEST_KEY PP_WP7_PHYSICAL_RACING_TEST)
    require_list_has(${role}_FORBIDDEN_DEFINES "${flag}")
  endforeach()
endforeach()

# A bad selection must fail rather than falling back to the accepted integrated
# graph.  This is deliberately CMake script mode, not an ESP-IDF configure.
execute_process(
  COMMAND "${CMAKE_COMMAND}"
          "-DPP_S4_PROFILE_FILE=${PP_ROOT}/firmware/wp7_idf_arduino_dbuf/cmake/pp_s4_role_profiles.cmake"
          -P "${CMAKE_CURRENT_LIST_DIR}/s4_role_profile_invalid_selection.cmake"
  RESULT_VARIABLE invalid_selection_result
  OUTPUT_QUIET
  ERROR_QUIET)
if(invalid_selection_result EQUAL 0)
  message(FATAL_ERROR "S4b role-profile check failed: invalid profile selection did not fail")
endif()

message(STATUS "S4b independent role build-profile contract PASS")
