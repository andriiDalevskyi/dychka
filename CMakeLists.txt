cmake_minimum_required(VERSION 3.22)

project(Dychka VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external/JUCE)

juce_add_plugin(Dychka
    COMPANY_NAME "Dallas Audio"
    BUNDLE_ID "com.dallasaudio.dychka"
    PLUGIN_MANUFACTURER_CODE Dlas
    PLUGIN_CODE Dych
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_512.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_64.png"
    PRODUCT_NAME "Dychka")

juce_generate_juce_header(Dychka)

file(GLOB_RECURSE DYCHKA_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

target_sources(Dychka PRIVATE ${DYCHKA_SOURCES})

target_include_directories(Dychka PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(Dychka
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Dychka
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline engine test (console app): envelope maths, JSON round trips, factory data and the
# time / volume engine on synthetic signals (pass-through, delay, half-time, reverse, stutter,
# gate, sync, MIDI). Exit code 0 = pass.
# Build: cmake --build build --config Release --target DychkaEngineTest
option(DYCHKA_BUILD_TESTS "Build the offline engine test" ON)
if(DYCHKA_BUILD_TESTS)
    juce_add_console_app(DychkaEngineTest PRODUCT_NAME "DychkaEngineTest")
    target_sources(DychkaEngineTest PRIVATE
        tests/EngineTest.cpp
        src/Engine/DychkaEngine.cpp
        src/Model/FactoryEnvelopes.cpp
        src/Model/BankStore.cpp)
    target_include_directories(DychkaEngineTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(DychkaEngineTest PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=0)
    target_link_libraries(DychkaEngineTest PRIVATE
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
