cmake_minimum_required(VERSION 3.22)

# Set the deployment target before project() selects the macOS SDK. Apple
# automatically uses 11.0 as the minimum for the arm64 slice.
if(CMAKE_HOST_APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
endif()

project(GateVerb VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GATEVERB_BUILD_TESTS "Build DSP and processor regression checks" ON)
option(GATEVERB_DSP_TESTS_ONLY "Build the dependency-free DSP checks only" OFF)

include(CTest)
if(GATEVERB_BUILD_TESTS)
    add_executable(GateVerbDspTests Tests/DspTests.cpp Source/DspEngine.cpp)
    target_include_directories(GateVerbDspTests PRIVATE Source)
    add_test(NAME dsp COMMAND GateVerbDspTests)
    set_tests_properties(dsp PROPERTIES TIMEOUT 90)
endif()

if(GATEVERB_DSP_TESTS_ONLY)
    return()
endif()

include(FetchContent)
# Fixed JUCE 8 security release and checksum; no moving dependency branches.
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.17-security.tar.gz
    URL_HASH SHA256=d37bab9f2330c50f42ba979575f6b55d5d3b23fa94221c2e57cac3abb69ac6d9
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(GateVerb
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "studiobro.net"
    COMPANY_WEBSITE "https://studiobro.net"
    COMPANY_COPYRIGHT "Copyright (c) 2026 M. Oliver Ullmann"
    BUNDLE_ID "net.studiobro.gateverb"
    PLUGIN_MANUFACTURER_CODE Stbr
    PLUGIN_CODE Gtvb
    FORMATS VST3
    PRODUCT_NAME "GATE-VERB"
    DESCRIPTION "GATE-VERB by studiobro.net for Sound&Recording Magazin"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Reverb)

juce_generate_juce_header(GateVerb)
target_sources(GateVerb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DspEngine.cpp)
target_include_directories(GateVerb PRIVATE Source)
target_compile_definitions(GateVerb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)
target_link_libraries(GateVerb PRIVATE
    juce::juce_audio_processors
    juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_warning_flags)

if(GATEVERB_BUILD_TESTS)
    # Reuse the exact compiled processor/editor objects from the VST3 target.
    # Only this small main() is compiled again; JUCE is not built twice.
    add_executable(GateVerbProcessorTests Tests/ProcessorTests.cpp)
    target_link_libraries(GateVerbProcessorTests PRIVATE GateVerb)
    target_include_directories(GateVerbProcessorTests PRIVATE
        $<TARGET_GENEX_EVAL:GateVerb,$<TARGET_PROPERTY:GateVerb,INCLUDE_DIRECTORIES>>)
    target_compile_definitions(GateVerbProcessorTests PRIVATE
        $<TARGET_GENEX_EVAL:GateVerb,$<TARGET_PROPERTY:GateVerb,COMPILE_DEFINITIONS>>)
    add_dependencies(GateVerbProcessorTests GateVerb)
    add_test(NAME processor COMMAND GateVerbProcessorTests)
    set_tests_properties(processor PROPERTIES TIMEOUT 90)
endif()
