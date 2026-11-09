// SeqCalc editor — mirrors the Max device layout: the 6 action
// buttons, the 12-button grid, the PC-set / Forte / pattern text
// fields, the scale menus, the behaviour toggles, CalcS / SEND /
// STOP, a note display and piano strip, and the MIDI drag handle.
// setSize() is called last, after every child exists.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <string>
#include <vector>

#include "PluginProcessor.h"

class PianoStrip : public juce::Component {
public:
    void setNotes(const std::vector<int>& notes);
    void paint(juce::Graphics& g) override;

private:
    std::vector<int> notes_;
};

// A child-component drag handle: performExternalDragDropOfFiles
// resolves the DragAndDropContainer from the SOURCE's parents, so
// the source must be a child of the editor, never the editor itself.
class DragHandleButton : public juce::TextButton {
public:
    explicit DragHandleButton(SeqCalcProcessor& processor);
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    SeqCalcProcessor& processor_;
    bool dragStarted_ = false;
    juce::Point<int> dragStart_;
};

class SeqCalcEditor : public juce::AudioProcessorEditor,
                      public juce::DragAndDropContainer,
                      private juce::Timer {
public:
    explicit SeqCalcEditor(SeqCalcProcessor& processor);
    ~SeqCalcEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    void refreshGridButtons();
    void refreshActionButtons();
    void commitPcSetText();
    void commitForteText();
    void commitPatternText();

    SeqCalcProcessor& processor_;

    juce::Label titleLabel_;
    juce::Label creditLabel_;
    juce::Label staleLabel_;
    juce::Label noteDisplayLabel_;
    juce::Label bpmLabel_;
    juce::Label pcSetLabel_;
    juce::Label forteLabel_;
    juce::Label patternLabel_;
    juce::Label subdivLabel_;
    juce::Label repeatLabel_;
    juce::Label scaleLabel_;

    std::array<juce::TextButton, 6> actionButtons_;
    std::array<juce::TextButton, 12> gridButtons_;
    juce::TextEditor pcSetEditor_;
    juce::TextEditor forteEditor_;
    juce::TextEditor patternEditor_;
    juce::ComboBox rootCombo_;
    juce::ComboBox baseOctaveCombo_;
    juce::ComboBox scaleRootCombo_;
    juce::ComboBox scaleOctaveCombo_;
    juce::ComboBox scaleCombo_;
    juce::ComboBox subdivCombo_;
    juce::ComboBox repeatCombo_;
    juce::ToggleButton shuffleToggle_;
    juce::ToggleButton randomInScaleToggle_;
    juce::ToggleButton rootOnlyToggle_;
    juce::ToggleButton restsForRToggle_;
    juce::ToggleButton usePcSetToggle_;
    juce::TextButton calcButton_;
    juce::TextButton sendButton_;
    juce::TextButton stopButton_;
    juce::TextButton clearSetButton_;
    juce::TextButton resetButton_;
    juce::Slider bpmSlider_;
    PianoStrip pianoStrip_;
    DragHandleButton dragHandle_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> baseOctaveAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleRootAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleOctaveAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> subdivAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> repeatAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> shuffleAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> randomInScaleAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> rootOnlyAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> restsForRAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> usePcSetAttach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAttach_;

    std::string lastDisplay_;
    std::string lastError_;
    bool lastStale_ = false;
    bool lastPlaying_ = false;
    std::vector<int> lastNotes_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SeqCalcEditor)
};
