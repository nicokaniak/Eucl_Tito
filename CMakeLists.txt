cmake_minimum_required(VERSION 3.21)

project(Eucl_Tito VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(JUCE_SOURCE_DIR "" CACHE PATH "Path to a local JUCE checkout (leave empty to fetch from GitHub)")
if(JUCE_SOURCE_DIR)
    if(NOT EXISTS "${JUCE_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "JUCE_SOURCE_DIR does not point to a JUCE checkout")
    endif()
    add_subdirectory(${JUCE_SOURCE_DIR} JUCE)
    set(JUCE_MODULES_DIR "${JUCE_SOURCE_DIR}/modules")
else()
    FetchContent_Declare(
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.3
    )
    FetchContent_MakeAvailable(juce)
    set(JUCE_MODULES_DIR "${juce_SOURCE_DIR}/modules")
endif()

set(VST3_SDK_DIR "" CACHE PATH "Optional override for the VST3 SDK location")
if(VST3_SDK_DIR)
    juce_set_vst3_path(${VST3_SDK_DIR})
elseif(EXISTS "${JUCE_MODULES_DIR}/juce_audio_processors_headless/format_types/VST3_SDK")
    juce_set_vst3_path("${JUCE_MODULES_DIR}/juce_audio_processors_headless/format_types/VST3_SDK")
endif()

juce_add_plugin(Eucl_Tito
    COMPANY_NAME "yourcompany"
    PRODUCT_NAME "Eucl_Tito"
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "Eucl_Tito"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    PRODUCES_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE ManU
    PLUGIN_CODE Jf65
    FORMATS VST3 Standalone
    VST3_CATEGORIES "Fx|Generator"
)

juce_generate_juce_header(Eucl_Tito)

target_sources(Eucl_Tito
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

if(MSVC)
    target_sources(Eucl_Tito PRIVATE Builds/VisualStudio2026/resources.rc)
endif()

target_compile_definitions(Eucl_Tito
    PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_DISPLAY_SPLASH_SCREEN=0
        _CRT_SECURE_NO_WARNINGS
)

set(_eucl_tito_common_modules
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_plugin_client
    juce::juce_audio_formats
    juce::juce_audio_devices
    juce::juce_audio_basics
    juce::juce_gui_extra
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_events
    juce::juce_data_structures
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

if(TARGET juce::juce_audio_processors_headless)
    list(APPEND _eucl_tito_common_modules juce::juce_audio_processors_headless)
else()
    message(STATUS "juce_audio_processors_headless module not found; build will proceed without it")
endif()

target_link_libraries(Eucl_Tito
    PRIVATE
        ${_eucl_tito_common_modules}
)

juce_add_console_app(Eucl_Tito_VST3ManifestHelper
    PRODUCT_NAME "juce_vst3_helper"
    COMPANY_NAME "yourcompany"
    VERSION ${PROJECT_VERSION}
)

set(_manifest_helper_source "${JUCE_MODULES_DIR}/juce_audio_plugin_client/VST3/juce_VST3ManifestHelper.cpp")
if(NOT EXISTS "${_manifest_helper_source}")
    message(FATAL_ERROR "Unable to locate juce_VST3ManifestHelper.cpp. Make sure JUCE_MODULES_DIR is set correctly.")
endif()

target_sources(Eucl_Tito_VST3ManifestHelper PRIVATE ${_manifest_helper_source})

target_compile_definitions(Eucl_Tito_VST3ManifestHelper
    PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_DISPLAY_SPLASH_SCREEN=0
        _CRT_SECURE_NO_WARNINGS
)

target_link_libraries(Eucl_Tito_VST3ManifestHelper
    PRIVATE
        juce::juce_audio_plugin_client
        ${_eucl_tito_common_modules}
        juce::juce_recommended_warning_flags
)
