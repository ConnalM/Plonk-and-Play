idf_component_register(
  SRCS taster_runtime_esp32.cpp ../src/system_time.cpp
  INCLUDE_DIRS ../include
  REQUIRES arduino gfx tamc esp_wifi esp_event esp_netif esp_timer
           esp_partition esp_driver_gpio esp_driver_i2c esp_lcd freertos
           mbedtls nvs_flash spi_flash)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
  PP_S4_PRODUCTION_TASTER=1
  PP_WP7_SINGLE_FB_RECOVERY=1
  PP_WP7_DISABLE_ANIMATIONS=1
  PP_WP7_STATIC_WELCOME_RASTER=1
  ARDUINO_USB_CDC_ON_BOOT=0)
# Binary artwork is assembled by this component as well as its C++ sources.
# The role guard is deliberately a C++ header, so apply it only where that
# grammar is valid; the role definition above still reaches every compilation
# unit, including the generated asset assembly.
target_compile_options(${COMPONENT_LIB} PRIVATE
  "$<$<COMPILE_LANGUAGE:C,CXX>:-include;${CMAKE_CURRENT_LIST_DIR}/../include/pp/s4_production_role_guard.h>")

set(PP_TASTER_ASSET_ROOT "${CMAKE_CURRENT_LIST_DIR}/../assets")
target_add_binary_data(${COMPONENT_LIB}
  "${PP_TASTER_ASSET_ROOT}/wp7_welcome_static_rgb565.bin" BINARY)
foreach(PP_TASTER_ARTWORK
    lamp_red.rgb565 lamp_green.rgb565 lamp_off.rgb565 lamp_yellow.rgb565
    header_trapezoid_surround.rgb565 pp_header_logo.rgb565)
  target_add_binary_data(${COMPONENT_LIB}
    "${PP_TASTER_ASSET_ROOT}/wp7_approved/${PP_TASTER_ARTWORK}" BINARY)
endforeach()
