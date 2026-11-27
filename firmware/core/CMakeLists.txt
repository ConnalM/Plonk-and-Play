# This component is intentionally not selected by the integrated project.
# A future, separately approved CORE configuration selects it after generated
# target evidence proves the effective dependency graph and forced guard.

idf_component_register(
  SRCS core_runtime_esp32.cpp
  INCLUDE_DIRS ../include
  REQUIRES
    arduino
    app_update
    esp_http_server
    esp_wifi
    esp_event
    esp_netif
    esp_timer
    esp_partition
    esp_driver_gpio
    esp_driver_i2c
    esp_driver_spi
    fatfs
    freertos
    mbedtls
    nvs_flash
    sdmmc
    spi_flash)

# This is source-level wiring only.  A future configure gate must inspect the
# generated command database and Ninja rules before treating it as proven.
target_compile_definitions(${COMPONENT_LIB} PRIVATE
  PP_S4_PRODUCTION_CORE=1
  PP_MAX_ENTRIES=2
  PP_MAX_SSE_CLIENTS=4)
target_compile_options(${COMPONENT_LIB} PRIVATE
  -include "${CMAKE_CURRENT_LIST_DIR}/../include/pp/s4_production_role_guard.h")
