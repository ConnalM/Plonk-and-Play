file(TO_CMAKE_PATH "$ENV{PP_GFX_ROOT}" PP_GFX_ROOT)
if(NOT IS_DIRECTORY "${PP_GFX_ROOT}/src")
  message(FATAL_ERROR "PP_GFX_ROOT must point to the Arduino_GFX library")
endif()

idf_component_register(
  SRCS
    "${PP_GFX_ROOT}/src/Arduino_DataBus.cpp"
    "${PP_GFX_ROOT}/src/Arduino_G.cpp"
    "${PP_GFX_ROOT}/src/Arduino_GFX_Library.cpp"
    "${PP_GFX_ROOT}/src/Arduino_GFX.cpp"
    "${PP_GFX_ROOT}/src/Arduino_TFT.cpp"
    "${PP_GFX_ROOT}/src/Arduino_TFT_18bit.cpp"
    "${PP_GFX_ROOT}/src/databus/Arduino_ESP32RGBPanel.cpp"
    "${PP_GFX_ROOT}/src/display/Arduino_RGB_Display.cpp"
  INCLUDE_DIRS "${PP_GFX_ROOT}/src"
  REQUIRES arduino esp_lcd)

# Arduino_GFX intentionally overloads flush(bool), which hides Arduino
# Print::flush().  Keep the third-party source unchanged in this opt-in build.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=overloaded-virtual)
