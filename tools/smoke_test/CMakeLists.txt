add_executable(SeqCalcSmokeTest main.cpp)

target_include_directories(SeqCalcSmokeTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/core
)

target_sources(SeqCalcSmokeTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src/PluginProcessor.cpp
    ${CMAKE_SOURCE_DIR}/src/PluginEditor.cpp
    ${CMAKE_SOURCE_DIR}/core/ForteTable.cpp
    ${CMAKE_SOURCE_DIR}/core/SetTheory.cpp
    ${CMAKE_SOURCE_DIR}/core/PatternEngine.cpp
    ${CMAKE_SOURCE_DIR}/core/ClipRenderer.cpp
)

target_compile_definitions(SeqCalcSmokeTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    SEQCALC_VERSION="0.1.0"
)

target_link_libraries(SeqCalcSmokeTest PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

add_custom_command(TARGET SeqCalcSmokeTest POST_BUILD
    COMMAND $<TARGET_FILE:SeqCalcSmokeTest>
    COMMENT "Running SeqCalc smoke test"
)
