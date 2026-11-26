cmake_minimum_required(VERSION 3.16)

get_filename_component(PP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${PP_ROOT}/firmware/wp7_idf_arduino_dbuf/cmake/pp_s4_role_profiles.cmake")

pp_s4_validate_role_forced_guard("CORE" "${PP_S4_TEST_FORCED_GUARD_HEADERS}")
