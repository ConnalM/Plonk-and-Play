file(TO_CMAKE_PATH "$ENV{PP_TAMC_ROOT}" PP_TAMC_ROOT)
if(NOT EXISTS "${PP_TAMC_ROOT}/TAMC_GT911.cpp")
  message(FATAL_ERROR "PP_TAMC_ROOT must point to the TAMC_GT911 library")
endif()

idf_component_register(
  SRCS "${PP_TAMC_ROOT}/TAMC_GT911.cpp"
  INCLUDE_DIRS "${PP_TAMC_ROOT}"
  REQUIRES arduino)

# This is the unchanged production TAMC_GT911 source.  Its legacy checksum
# local is not initialised, and ESP-IDF builds this opt-in target with
# -Werror=all.  Keep the dependency unchanged while preventing that warning
# from blocking the isolated integration build.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=maybe-uninitialized)
