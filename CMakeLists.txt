cmake_minimum_required(VERSION 3.22)
project(Fourpole VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
  message(FATAL_ERROR "JUCE not found. Run scripts/bootstrap.sh first.")
endif()
add_subdirectory(external/JUCE)

# Core DSP: compiled into both the app and the test runner.
set(FOURPOLE_CORE_SOURCES
  src/core/Dsp.cpp
  src/core/Synth.cpp
  src/core/Patch.cpp
  src/core/MidiMap.cpp
  src/core/PanelControls.cpp
)

set(FOURPOLE_JUCE_DEFS
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1
)

option(FOURPOLE_BUILD_APP "Build the GUI app" ON)

if(FOURPOLE_BUILD_APP)
juce_add_gui_app(Fourpole
  PRODUCT_NAME "Fourpole"
  COMPANY_NAME "Fourpole Research"
  BUNDLE_ID "org.fourpole.app"
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "Fourpole uses audio input 1 as its EXTERNAL INPUT when you choose to plug it in.")

target_sources(Fourpole PRIVATE
  ${FOURPOLE_CORE_SOURCES}
  src/engine/Engine.cpp
  src/ui/Materials.cpp
  src/ui/Theme.cpp
  src/ui/Controls.cpp
  src/ui/Keyboard.cpp
  src/ui/MainComponent.cpp
  src/Main.cpp)

target_include_directories(Fourpole PRIVATE src)
target_compile_definitions(Fourpole PRIVATE ${FOURPOLE_JUCE_DEFS} JUCE_APPLICATION_NAME_STRING="Fourpole"
  JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
target_link_libraries(Fourpole PRIVATE
  juce::juce_audio_utils
  juce::juce_audio_devices
  juce::juce_gui_extra
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags)
endif()

# ---------------------------------------------------------------- tests
juce_add_console_app(FourpoleTests PRODUCT_NAME "FourpoleTests")
target_sources(FourpoleTests PRIVATE
  ${FOURPOLE_CORE_SOURCES}
  tests/TestMain.cpp
  tests/DspTests.cpp
  tests/SynthTests.cpp
  tests/ControlTests.cpp)
target_include_directories(FourpoleTests PRIVATE src)
target_compile_definitions(FourpoleTests PRIVATE ${FOURPOLE_JUCE_DEFS})
target_link_libraries(FourpoleTests PRIVATE
  juce::juce_core
  juce::juce_audio_basics
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)

enable_testing()
add_test(NAME FourpoleTests COMMAND FourpoleTests)

