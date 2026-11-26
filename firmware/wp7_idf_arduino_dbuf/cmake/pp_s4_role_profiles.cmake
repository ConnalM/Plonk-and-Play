# Board-independent S4b role build definitions.
#
# These manifests deliberately do not create a Core or Taster image yet.  The
# accepted integrated image remains the sole linkable profile.  Core and
# Taster have neither approved board/entry-point extraction nor a qualified
# production dependency graph, so selecting either one must fail before the
# ESP-IDF project is configured.  Keeping the intended lists here makes that
# absence explicit without quietly falling back to the integrated image.

function(pp_s4_role_is_valid role result)
  if(role STREQUAL "INTEGRATED" OR role STREQUAL "CORE" OR role STREQUAL "TASTER")
    set(${result} TRUE PARENT_SCOPE)
  else()
    set(${result} FALSE PARENT_SCOPE)
  endif()
endfunction()

function(pp_s4_select_role role prefix)
  string(TOUPPER "${role}" normalized_role)
  pp_s4_role_is_valid("${normalized_role}" valid)
  if(NOT valid)
    message(FATAL_ERROR
      "PP_S4_BUILD_ROLE must be INTEGRATED, CORE or TASTER; got '${role}'")
  endif()

  # Every profile states a provisional source, component and asset declaration.
  # These declarations are not yet consumed by an independent ESP-IDF target.
  # The two future-role probe sources are compile-only boundaries, not
  # application entry points and not substitute sporting services.
  if(normalized_role STREQUAL "INTEGRATED")
    set(linkable TRUE)
    set(identity "integrated-wp7-accepted")
    set(sources "src/main.cpp;src/system_time.cpp;src/wifi_scan_instrumentation.cpp")
    set(component_dirs "components/arduino;components/gfx;components/tamc")
    set(requires
      "arduino;gfx;tamc;app_update;esp_http_server;esp_wifi;esp_event;esp_netif;esp_timer;esp_partition;esp_driver_gpio;esp_driver_i2c;esp_driver_spi;esp_lcd;esp_mm;fatfs;freertos;mbedtls;nvs_flash;sdmmc;spi_flash")
    set(assets
      "assets/wp7_welcome_static_rgb565.bin;assets/wp7_approved/lamp_red.rgb565;assets/wp7_approved/lamp_green.rgb565;assets/wp7_approved/lamp_off.rgb565;assets/wp7_approved/lamp_yellow.rgb565;assets/wp7_approved/header_trapezoid_surround.rgb565;assets/wp7_approved/pp_header_logo.rgb565")
    set(forbidden_defines "")
  elseif(normalized_role STREQUAL "CORE")
    set(linkable FALSE)
    set(identity "core-scaffold-not-linkable")
    set(sources "src/wp8_core_role_compile_probe.cpp")
    set(component_dirs "components/arduino")
    set(requires
      "arduino;app_update;esp_http_server;esp_wifi;esp_event;esp_netif;esp_timer;esp_partition;esp_driver_gpio;esp_driver_spi;esp_mm;fatfs;freertos;mbedtls;nvs_flash;sdmmc;spi_flash")
    set(assets "")
    set(forbidden_defines
      "PP_PHASE8B_P1_TEST;PP_WP7_ACCEPTANCE_SIMULATION;PP_SIMULATED_RACING_TEST;PP_UPDATE_TEST_KEY;PP_WP7_PHYSICAL_RACING_TEST")
  else()
    set(linkable FALSE)
    set(identity "taster-scaffold-not-linkable")
    set(sources "src/wp8_taster_role_compile_probe.cpp")
    set(component_dirs "components/arduino;components/gfx;components/tamc")
    set(requires
      "arduino;gfx;tamc;esp_wifi;esp_event;esp_netif;esp_timer;esp_partition;esp_driver_gpio;esp_driver_i2c;esp_lcd;freertos;mbedtls;nvs_flash;spi_flash")
    set(assets
      "assets/wp7_welcome_static_rgb565.bin;assets/wp7_approved/lamp_red.rgb565;assets/wp7_approved/lamp_green.rgb565;assets/wp7_approved/lamp_off.rgb565;assets/wp7_approved/lamp_yellow.rgb565;assets/wp7_approved/header_trapezoid_surround.rgb565;assets/wp7_approved/pp_header_logo.rgb565")
    set(forbidden_defines
      "PP_PHASE8B_P1_TEST;PP_WP7_ACCEPTANCE_SIMULATION;PP_SIMULATED_RACING_TEST;PP_UPDATE_TEST_KEY;PP_WP7_PHYSICAL_RACING_TEST")
  endif()

  foreach(field ROLE LINKABLE IDENTITY SOURCES COMPONENT_DIRS REQUIRES ASSETS FORBIDDEN_DEFINES)
    string(TOLOWER "${field}" local_name)
    set(${prefix}_${field} "${${local_name}}" PARENT_SCOPE)
  endforeach()
  set(${prefix}_ROLE "${normalized_role}" PARENT_SCOPE)
endfunction()
