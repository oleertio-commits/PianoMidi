cmake_minimum_required(VERSION 3.22)
project(PianoMidi VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Runtime estatico: no hace falta instalar el VC++ Redistributable en el PC del usuario
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(PianoMidi
    COMPANY_NAME "Oleertio"
    PLUGIN_MANUFACTURER_CODE Olee
    PLUGIN_CODE Pmid
    FORMATS VST3
    PRODUCT_NAME "PianoMidi"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Analyzer
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(PianoMidi PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(PianoMidi PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_compile_options(PianoMidi PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(PianoMidi
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
