cmake_minimum_required(VERSION 3.22)
project(SeqCalc VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(SeqCalcVST
    VERSION 0.1.0
    COMPANY_NAME Zetsumetsu
    COMPANY_WEBSITE https://github.com/Zetsumetsu
    PRODUCT_NAME "SeqCalc"
    DESCRIPTION "Set-theory sequencer / calculator"
    PLUGIN_NAME "SeqCalc"
    PLUGIN_MANUFACTURER_CODE Zets
    PLUGIN_CODE Seq1
    FORMATS VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument
    VST3_SUBCATEGORIES "Instrument|Synth"
)

target_sources(SeqCalcVST PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    core/ForteTable.cpp
    core/SetTheory.cpp
    core/PatternEngine.cpp
    core/ClipRenderer.cpp
)

target_include_directories(SeqCalcVST PRIVATE src core)

target_compile_definitions(SeqCalcVST PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    SEQCALC_VERSION="0.1.0"
)

target_link_libraries(SeqCalcVST PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

option(SEQCALC_BUILD_SMOKE_TEST "Build the headless plugin smoke test" OFF)
if(SEQCALC_BUILD_SMOKE_TEST)
    add_subdirectory(tools/smoke_test)
endif()
