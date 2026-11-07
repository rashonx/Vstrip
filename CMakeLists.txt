cmake_minimum_required(VERSION 3.22)
project(RashonVocalSitter VERSION 1.0.0)

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

juce_add_plugin(VocalSitter
  COMPANY_NAME "Rashon X"
  BUNDLE_ID com.rashonx.vocalsitter
  PRODUCT_NAME "Rashon Vocal Sitter"
  PLUGIN_MANUFACTURER_CODE Vstr
  PLUGIN_CODE Rvs1
  FORMATS VST3 Standalone
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  VST3_CATEGORIES Fx EQ
  COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(SitterAssets SOURCES Assets/dodge_city_logo.png)

target_sources(VocalSitter PRIVATE
  Source/BandCarver.cpp
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(VocalSitter PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(VocalSitter
  PRIVATE SitterAssets juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

option(SITTER_BUILD_TESTS "Build the smoke test" OFF)
if(SITTER_BUILD_TESTS)
  add_executable(SitterSmokeTest Tests/SmokeTest.cpp)
  # Reuse the plugin's compiled JUCE code, include paths and settings
  target_include_directories(SitterSmokeTest PRIVATE $<TARGET_PROPERTY:VocalSitter,INCLUDE_DIRECTORIES>)
  target_compile_definitions(SitterSmokeTest PRIVATE $<TARGET_PROPERTY:VocalSitter,COMPILE_DEFINITIONS>)
  target_link_libraries(SitterSmokeTest PRIVATE VocalSitter)

  add_executable(SitterScreenshot Tests/Screenshot.cpp)
  target_include_directories(SitterScreenshot PRIVATE $<TARGET_PROPERTY:VocalSitter,INCLUDE_DIRECTORIES>)
  target_compile_definitions(SitterScreenshot PRIVATE $<TARGET_PROPERTY:VocalSitter,COMPILE_DEFINITIONS>)
  target_link_libraries(SitterScreenshot PRIVATE VocalSitter)
endif()
