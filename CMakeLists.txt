cmake_minimum_required(VERSION 3.22)
project(VocalStrip VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  # Static runtime so the plugin works on PCs without the VC++ redistributable
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 9.0.5
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(VocalStrip
  COMPANY_NAME "VocalStrip"
  PRODUCT_NAME "VocalStrip"
  PLUGIN_MANUFACTURER_CODE Vstr
  PLUGIN_CODE Vcs1
  FORMATS VST3 Standalone
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  VST3_CATEGORIES Fx Dynamics EQ
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(VocalStrip PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(VocalStrip PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(VocalStrip
  PRIVATE juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

option(VOCALSTRIP_BUILD_TESTS "Build the smoke test" OFF)
if(VOCALSTRIP_BUILD_TESTS)
  add_executable(VocalStripSmokeTest Tests/SmokeTest.cpp)
  # Reuse the plugin's compiled JUCE code, include paths and settings
  target_include_directories(VocalStripSmokeTest PRIVATE $<TARGET_PROPERTY:VocalStrip,INCLUDE_DIRECTORIES>)
  target_compile_definitions(VocalStripSmokeTest PRIVATE $<TARGET_PROPERTY:VocalStrip,COMPILE_DEFINITIONS>)
  target_link_libraries(VocalStripSmokeTest PRIVATE VocalStrip)
endif()
