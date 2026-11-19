cmake_minimum_required(VERSION 3.24)

# ==============================================================================
# Plugin identity -- everything that names the plugin lives in this block.
# NOTE: the manufacturer/plugin codes and the bundle id identify the plugin to
# DAWs. Changing them after release makes existing projects lose the plugin.
# The manufacturer code is shared with IRene (repo anschae/irene).
# ==============================================================================
set(PD_PLUGIN_NAME        "Prog Delay")
set(PD_TARGET             "ProgDelay")
set(PD_VERSION            "0.1.0")
set(PD_COMPANY_NAME       "anschae")
set(PD_MANUFACTURER_CODE  "Ansc")   # same as IRene
set(PD_PLUGIN_CODE        "PrDl")
set(PD_BUNDLE_ID          "ch.anschae.progdelay")
set(PD_JUCE_TAG           "8.0.15")  # pinned JUCE release
set(PD_CATCH2_TAG         "v3.16.1") # pinned Catch2 release

# ==============================================================================
# Platform settings (must be set before project())
# ==============================================================================
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures (universal)")
# Static MSVC runtime: no VC++ redistributable needed on the user's machine.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

project(ProgDelay VERSION ${PD_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_XCODE_GENERATE_SCHEME OFF)

option(PD_BUILD_TESTS  "Build the Catch2 unit tests" ON)
option(PD_BUILD_PLUGIN "Build the plugin targets (VST3/AU/Standalone)" ON)
option(PD_BUILD_TOOLS  "Build developer tools (UI snapshot renderer)" OFF)
# MIDI pattern switching. OFF by default so the plugin stays a plain audio
# effect. ON makes the AU a MIDI-controlled effect ('aumf'), which is what Logic
# needs to deliver MIDI to an effect -- note that this changes the AU type, so
# Logic treats it as a different plugin.
option(PD_MIDI_SWITCHING "Allow pattern switching via MIDI program change / notes" OFF)

# ==============================================================================
# Dependencies
# ==============================================================================
include(FetchContent)
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${PD_JUCE_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   FALSE)
FetchContent_MakeAvailable(juce)

if(PD_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        ${PD_CATCH2_TAG}
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   FALSE)
    FetchContent_MakeAvailable(Catch2)
endif()

# ==============================================================================
# Shared code: compiled into the plugin, the unit tests and the tools.
# (An INTERFACE library, because JUCE modules must be compiled once per final
# target; a static library would duplicate JUCE symbols.)
# ==============================================================================
add_library(PDShared INTERFACE)

file(GLOB_RECURSE PD_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
list(FILTER PD_SOURCES EXCLUDE REGEX ".*/src/PluginEntry\\.cpp$")
target_sources(PDShared INTERFACE ${PD_SOURCES})

target_include_directories(PDShared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(PD_MIDI_SWITCHING)
    set(PD_MIDI_FLAG 1)
else()
    set(PD_MIDI_FLAG 0)
endif()

target_compile_definitions(PDShared INTERFACE
    PD_PLUGIN_NAME="${PD_PLUGIN_NAME}"
    PD_COMPANY_NAME="${PD_COMPANY_NAME}"
    PD_VERSION_STRING="${PD_VERSION}"
    PD_MIDI_SWITCHING=${PD_MIDI_FLAG}
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(PDShared INTERFACE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)

if(MSVC)
    target_compile_options(PDShared INTERFACE /W4 /bigobj)
else()
    target_compile_options(PDShared INTERFACE -Wall -Wextra)
endif()

# ==============================================================================
# Plugin
# ==============================================================================
if(PD_BUILD_PLUGIN)
    set(PD_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND PD_FORMATS AU)
    endif()

    if(PD_MIDI_SWITCHING)
        set(PD_AU_TYPE kAudioUnitType_MusicEffect)  # 'aumf'
        set(PD_NEEDS_MIDI TRUE)
    else()
        set(PD_AU_TYPE kAudioUnitType_Effect)       # 'aufx'
        set(PD_NEEDS_MIDI FALSE)
    endif()

    juce_add_plugin(${PD_TARGET}
        PRODUCT_NAME                "${PD_PLUGIN_NAME}"
        COMPANY_NAME                "${PD_COMPANY_NAME}"
        BUNDLE_ID                   "${PD_BUNDLE_ID}"
        PLUGIN_MANUFACTURER_CODE    ${PD_MANUFACTURER_CODE}
        PLUGIN_CODE                 ${PD_PLUGIN_CODE}
        FORMATS                     ${PD_FORMATS}
        IS_SYNTH                    FALSE
        NEEDS_MIDI_INPUT            ${PD_NEEDS_MIDI}
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        AU_MAIN_TYPE                ${PD_AU_TYPE}
        VST3_CATEGORIES             Fx Delay
        COPY_PLUGIN_AFTER_BUILD     FALSE)

    target_sources(${PD_TARGET} PRIVATE src/PluginEntry.cpp)
    target_link_libraries(${PD_TARGET} PRIVATE PDShared)
    # Plugins must never run modal loops: they would block the host.
    target_compile_definitions(${PD_TARGET} PUBLIC JUCE_MODAL_LOOPS_PERMITTED=0)
endif()

# ==============================================================================
# Unit tests (Catch2)
# ==============================================================================
if(PD_BUILD_TESTS)
    enable_testing()

    juce_add_console_app(ProgDelayTests PRODUCT_NAME "ProgDelayTests")
    file(GLOB PD_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
                                                ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h)
    target_sources(ProgDelayTests PRIVATE ${PD_TEST_SOURCES})
    target_link_libraries(ProgDelayTests PRIVATE PDShared Catch2::Catch2)
    target_compile_definitions(ProgDelayTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

    add_test(NAME ProgDelayTests COMMAND ProgDelayTests)
endif()

# ==============================================================================
# Developer tools (not built by default)
# ==============================================================================
if(PD_BUILD_TOOLS)
    # Renders the editor to a PNG without a DAW: handy for checking layouts.
    juce_add_console_app(ProgDelaySnapshot PRODUCT_NAME "ProgDelaySnapshot")
    target_sources(ProgDelaySnapshot PRIVATE tools/Snapshot.cpp)
    target_link_libraries(ProgDelaySnapshot PRIVATE PDShared)
    target_compile_definitions(ProgDelaySnapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
endif()
