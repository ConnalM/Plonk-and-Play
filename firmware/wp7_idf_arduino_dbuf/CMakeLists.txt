cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/pp_s4_role_profiles.cmake)
set(PP_S4_BUILD_ROLE "INTEGRATED" CACHE STRING
    "S4b build profile: INTEGRATED, CORE or TASTER")
set_property(CACHE PP_S4_BUILD_ROLE PROPERTY STRINGS INTEGRATED CORE TASTER)
set(PP_S4_PRODUCTION_ROLE_DEFINES "" CACHE STRING
    "Future standalone-role definitions checked before ESP-IDF configuration")
set(PP_S4_COMPONENT_COMPILE_DEFINES "" CACHE STRING
    "Future transitive component definitions checked before role configuration")
set(PP_S4_GLOBAL_COMPILE_DEFINES "" CACHE STRING
    "Future global definitions checked before role configuration")
pp_s4_select_role("${PP_S4_BUILD_ROLE}" PP_S4_PROFILE)
pp_s4_validate_effective_role_definitions("${PP_S4_PROFILE_ROLE}"
  "${PP_S4_PRODUCTION_ROLE_DEFINES}"
  "${PP_S4_COMPONENT_COMPILE_DEFINES}"
  "${PP_S4_GLOBAL_COMPILE_DEFINES}")
pp_s4_validate_role_forced_guard("${PP_S4_PROFILE_ROLE}"
  "${PP_S4_PROFILE_FORCED_GUARD_HEADERS}")

# Core/Taster lists are deliberately non-linkable until approved entry points,
# board-independent service extraction and board facts exist.  Refuse early:
# a future role selection must never fall through to the integrated image.
if(NOT PP_S4_PROFILE_LINKABLE)
  message(FATAL_ERROR
    "PP_S4_BUILD_ROLE=${PP_S4_PROFILE_ROLE} is an S4b non-linkable scaffold; "
    "approved role entry points and board/service extraction are still required")
endif()

# Opt-in WP7 experiment: the production PlatformIO/Arduino target remains the
# recovery path.  This project deliberately uses the full ESP-IDF build so the
# esp_lcd RGB driver is compiled from source with CONFIG_LCD_RGB_RESTART_IN_VSYNC=n.
set(PP_ARDUINO_COMPONENT_DIR "$ENV{PP_ARDUINO_COMPONENT_DIR}")
if(NOT IS_DIRECTORY "${PP_ARDUINO_COMPONENT_DIR}")
  message(FATAL_ERROR "PP_ARDUINO_COMPONENT_DIR must point to the Arduino-ESP32 component")
endif()

set(EXTRA_COMPONENT_DIRS)
foreach(PP_S4_COMPONENT_DIR IN LISTS PP_S4_PROFILE_COMPONENT_DIRS)
  list(APPEND EXTRA_COMPONENT_DIRS
       "${CMAKE_CURRENT_LIST_DIR}/${PP_S4_COMPONENT_DIR}")
endforeach()

# Keep the target self-contained when invoked through idf.py from a fresh shell.
set(ENV{IDF_TARGET} esp32s3)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(plonk-and-play)

# Targeted workaround for the Xtensa GCC IRA crash seen while compiling the
# ESP-IDF RGB panel driver on Windows.  This is deliberately opt-in and applies
# only to the driver's translation unit; all application code and all other
# component compilation settings remain unchanged.
if("$ENV{PP_WP7_RGB_DRIVER_SIZE_OPT}" STREQUAL "1")
  idf_component_get_property(PP_WP7_ESP_LCD_LIB esp_lcd COMPONENT_LIB)
  get_target_property(PP_WP7_ESP_LCD_SOURCES
                      ${PP_WP7_ESP_LCD_LIB} SOURCES)
  set(PP_WP7_RGB_DRIVER_MATCHED FALSE)
  foreach(PP_WP7_RGB_SOURCE IN LISTS PP_WP7_ESP_LCD_SOURCES)
    if(PP_WP7_RGB_SOURCE MATCHES "esp_lcd_panel_rgb\\.c$")
      # The source belongs to ESP-IDF's component directory, so apply the
      # source property in that target's directory scope. A target-wide option
      # would incorrectly change all esp_lcd translation units.
      set_source_files_properties(
          "${PP_WP7_RGB_SOURCE}"
          TARGET_DIRECTORY ${PP_WP7_ESP_LCD_LIB}
          PROPERTIES COMPILE_FLAGS "-Os")
      set(PP_WP7_RGB_DRIVER_MATCHED TRUE)
      message(STATUS
              "WP7: compiling only ${PP_WP7_RGB_SOURCE} with -Os (targeted workaround)")
    endif()
  endforeach()
  if(NOT PP_WP7_RGB_DRIVER_MATCHED)
    message(FATAL_ERROR
            "PP_WP7_RGB_DRIVER_SIZE_OPT requested, but esp_lcd_panel_rgb.c was not found in esp_lcd sources")
  endif()
endif()
