# Fourpole as a module for hosts (the Tabletop studio): the sources to compile into the host's own target,
# which brings JUCE (juce_audio_utils, juce_audio_processors, juce_gui_extra). The app and the tests use the
# same lists.
set(FOURPOLE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(FOURPOLE_CORE_SOURCES
  ${FOURPOLE_DIR}/src/core/Dsp.cpp
  ${FOURPOLE_DIR}/src/core/Synth.cpp
  ${FOURPOLE_DIR}/src/core/Patch.cpp
  ${FOURPOLE_DIR}/src/core/MidiMap.cpp
  ${FOURPOLE_DIR}/src/core/PanelControls.cpp
  ${FOURPOLE_DIR}/src/core/History.cpp
  ${FOURPOLE_DIR}/src/core/PatchLibrary.cpp)
set(FOURPOLE_UI_SOURCES
  ${FOURPOLE_DIR}/src/engine/Engine.cpp
  ${FOURPOLE_DIR}/src/ui/Materials.cpp
  ${FOURPOLE_DIR}/src/ui/Theme.cpp
  ${FOURPOLE_DIR}/src/ui/Controls.cpp
  ${FOURPOLE_DIR}/src/ui/Keyboard.cpp
  ${FOURPOLE_DIR}/src/ui/MainComponent.cpp)
set(FOURPOLE_MODULE_SOURCES ${FOURPOLE_CORE_SOURCES} ${FOURPOLE_UI_SOURCES} ${FOURPOLE_DIR}/src/plugin/FourpoleProcessor.cpp)
