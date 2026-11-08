cmake_minimum_required(VERSION 3.22)
project(RashonSoulKeys VERSION 1.3.0)

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

juce_add_plugin(SoulKeys
  COMPANY_NAME "Dodge City Plugins"
  BUNDLE_ID com.rashonx.soulkeys
  PRODUCT_NAME "Rashon Soul Keys"
  PLUGIN_MANUFACTURER_CODE Vstr
  PLUGIN_CODE Rsk1
  FORMATS VST3 Standalone
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT TRUE
  VST3_CATEGORIES Instrument Synth
  COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(SoulKeysAssets SOURCES Assets/dodge_city_logo.png)

target_sources(SoulKeys PRIVATE
  Source/Chords.cpp
  Source/ChordExtractor.cpp
  Source/PresetFile.cpp
  Source/SoundEngine.cpp
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(SoulKeys PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_USE_MP3AUDIOFORMAT=1)

target_link_libraries(SoulKeys
  PRIVATE SoulKeysAssets juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

option(SOULKEYS_BUILD_TESTS "Build the smoke test and screenshot tool" OFF)
if(SOULKEYS_BUILD_TESTS)
  foreach(tool SmokeTest Screenshot)
    add_executable(SoulKeys${tool} Tests/${tool}.cpp)
    target_include_directories(SoulKeys${tool} PRIVATE $<TARGET_PROPERTY:SoulKeys,INCLUDE_DIRECTORIES>)
    target_compile_definitions(SoulKeys${tool} PRIVATE $<TARGET_PROPERTY:SoulKeys,COMPILE_DEFINITIONS>)
    target_link_libraries(SoulKeys${tool} PRIVATE SoulKeys)
  endforeach()
endif()
