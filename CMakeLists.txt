cmake_minimum_required(VERSION 3.21)
project(Subside VERSION 1.1.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DJUCE_VST3_CAN_REPLACE_VST2=0)
add_definitions(-DJUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1)

# ---- Vendored dependencies (no network needed) -----------------------------
add_subdirectory(JUCE)
add_subdirectory(clap-juce-extensions)

# ---- The plugin ------------------------------------------------------------
set(SYMPATHY_PLUGIN_FORMATS VST3)
if(APPLE)
    list(APPEND SYMPATHY_PLUGIN_FORMATS AU)
endif()

juce_add_plugin(Subside
    COMPANY_NAME "Dewpoint AV"
    COMPANY_WEBSITE "https://dewpoint.live"
    COMPANY_EMAIL "anthony@dewpoint.live"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Dwpt
    PLUGIN_CODE Subs
    FORMATS ${SYMPATHY_PLUGIN_FORMATS}
    PRODUCT_NAME "Subside"
    BUNDLE_ID "com.dewpoint.Subside"
)

clap_juce_extensions_plugin(TARGET Subside
    CLAP_ID "live.dewpoint.subside"
    CLAP_FEATURES audio-effect utility stereo)

juce_generate_juce_header(Subside)

# Embedded Monument Extended logotype fonts.
juce_add_binary_data(SubsideBinaryData
    SOURCES
        Source/MonumentExtendedUltrabold.otf
        Source/MonumentExtendedRegular.otf)

# ---- Source files ----------------------------------------------------------
target_sources(Subside
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DuckVisualizer.cpp
        Source/dsp/DuckEngine.cpp
)

target_include_directories(Subside
    PRIVATE
        Source
)

target_compile_definitions(Subside
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# ---- Link JUCE modules -----------------------------------------------------
target_link_libraries(Subside
    PRIVATE
        SubsideBinaryData
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_basics
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_data_structures
        juce::juce_events
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if(MSVC)
    target_compile_options(Subside PRIVATE /W4)
else()
    target_compile_options(Subside PRIVATE -Wall -Wextra)
endif()

# ---- Packaging / staging (Windows) -----------------------------------------
if(WIN32)
    add_custom_command(TARGET Subside_CLAP POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Subside_CLAP>"
            "${CMAKE_BINARY_DIR}/dist/Subside.clap"
        COMMENT "Staging Subside.clap")

    add_custom_command(TARGET Subside_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/Subside_artefacts/$<CONFIG>/VST3/Subside.vst3"
            "${CMAKE_BINARY_DIR}/dist/Subside.vst3"
        COMMENT "Staging Subside.vst3")
endif()
