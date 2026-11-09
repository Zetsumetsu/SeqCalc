// SeqCalc editor. See PluginEditor.h. Layout mirrors the Max device:
// action buttons, 12-button grid, text fields, scale menus, toggles,
// CalcS / SEND / STOP, note display, piano strip, drag handle.
// Magenta is the highlight colour (never orange/amber/copper).
#include "PluginEditor.h"

#include <algorithm>

#include "../core/ScaleLibrary.h"

namespace {

const juce::Colour kBg(0xff23272b);
const juce::Colour kPanel(0xff2b3036);
const juce::Colour kButton(0xff3a4048);
const juce::Colour kText(0xffe8e8e8);
const juce::Colour kDim(0xff9aa0a6);
const juce::Colour kMagenta(0xffff2ea6);
const juce::Colour kMagentaDark(0xffa31e6e);
const juce::Colour kKeyBlack(0xff15181a);

const char* const kNoteNames[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

const char* const kActionNames[6] = {
    "Root", "Roct", "Range", "PC", "Rhy", "Forte"
};

void styleButton(juce::TextButton& b) {
    b.setColour(juce::TextButton::buttonColourId, kButton);
    b.setColour(juce::TextButton::textColourOnId, kText);
    b.setColour(juce::TextButton::textColourOffId, kText);
}

void styleToggle(juce::ToggleButton& t) {
    t.setColour(juce::ToggleButton::textColourId, kText);
    t.setColour(juce::ToggleButton::tickColourId, kMagenta);
    t.setColour(juce::ToggleButton::tickDisabledColourId, kDim);
}

} // namespace

// --------------------------------------------------------------- PianoStrip
void PianoStrip::setNotes(const std::vector<int>& notes) {
    if (notes_ != notes) {
        notes_ = notes;
        repaint();
    }
}

void PianoStrip::paint(juce::Graphics& g) {
    g.fillAll(kPanel);
    const int w = getWidth();
    const int h = getHeight();
    const int firstNote = 21; // A0
    const int lastNote = 108; // C8
    int whiteCount = 0;
    for (int n = firstNote; n <= lastNote; ++n) {
        const int pc = n % 12;
        if (pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7 || pc == 9 || pc == 11)
            ++whiteCount;
    }
    const float whiteW = (float)w / (float)whiteCount;
    const float blackW = whiteW * 0.62f;
    const float blackH = (float)h * 0.62f;

    auto isWhite = [](int pc) {
        return pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7 || pc == 9 || pc == 11;
    };
    auto isHighlighted = [&](int n) {
        return std::find(notes_.begin(), notes_.end(), n) != notes_.end();
    };

    // White keys.
    int whiteIndex = 0;
    for (int n = firstNote; n <= lastNote; ++n) {
        if (!isWhite(n % 12)) continue;
        const float x = whiteIndex * whiteW;
        g.setColour(isHighlighted(n) ? kMagenta : juce::Colours::white);
        g.fillRect(x + 0.5f, 0.0f, whiteW - 1.0f, (float)h);
        g.setColour(kKeyBlack);
        g.drawRect(x + 0.5f, 0.0f, whiteW - 1.0f, (float)h, 1.0f);
        ++whiteIndex;
    }
    // Black keys.
    whiteIndex = 0;
    for (int n = firstNote; n <= lastNote; ++n) {
        const int pc = n % 12;
        if (isWhite(pc)) {
            ++whiteIndex;
            continue;
        }
        const float x = whiteIndex * whiteW - blackW * 0.5f;
        g.setColour(isHighlighted(n) ? kMagentaDark : kKeyBlack);
        g.fillRect(x, 0.0f, blackW, blackH);
    }
}

// ---------------------------------------------------------- DragHandleButton
DragHandleButton::DragHandleButton(SeqCalcProcessor& processor)
    : juce::TextButton("Drag MIDI to DAW"), processor_(processor) {
    setTooltip("Drag the calculated pattern to your DAW as a MIDI clip");
}

void DragHandleButton::mouseDown(const juce::MouseEvent& e) {
    dragStarted_ = false;
    dragStart_ = e.getPosition();
    juce::TextButton::mouseDown(e);
}

void DragHandleButton::mouseDrag(const juce::MouseEvent& e) {
    if (dragStarted_) return;
    if (e.getPosition().getDistanceFrom(dragStart_) < 5) return;
    dragStarted_ = true;
    auto file = processor_.exportMidiFile();
    if (file.existsAsFile()) {
        // The source is this button (a child of the editor), so
        // findParentDragContainerFor finds the editor container.
        // (JUCE 8 takes the file list as a StringArray.)
        juce::StringArray files;
        files.add(file.getFullPathName());
        juce::DragAndDropContainer::performExternalDragDropOfFiles(
            files, false, this);
    }
}

// ------------------------------------------------------------- SeqCalcEditor
SeqCalcEditor::SeqCalcEditor(SeqCalcProcessor& processor)
    : juce::AudioProcessorEditor(processor),
      processor_(processor),
      dragHandle_(processor) {
    setLookAndFeel(&juce::LookAndFeel::getDefaultLookAndFeel());

    titleLabel_.setText("SeqCalc", juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(26.0f, juce::Font::bold));
    titleLabel_.setColour(juce::Label::textColourId, kText);
    addAndMakeVisible(titleLabel_);

    creditLabel_.setText("set-theory sequencer / calculator  |  after Steve Zydek's SeqCalc",
                         juce::dontSendNotification);
    creditLabel_.setFont(juce::Font(13.0f));
    creditLabel_.setColour(juce::Label::textColourId, kDim);
    creditLabel_.setJustificationType(juce::Justification::right);
    addAndMakeVisible(creditLabel_);

    staleLabel_.setText("STALE - press CalcS", juce::dontSendNotification);
    staleLabel_.setFont(juce::Font(13.0f, juce::Font::bold));
    staleLabel_.setColour(juce::Label::textColourId, kMagenta);
    staleLabel_.setJustificationType(juce::Justification::right);
    staleLabel_.setVisible(false);
    addAndMakeVisible(staleLabel_);

    auto rowCaption = [&](juce::Label& l, const juce::String& text) {
        l.setText(text, juce::dontSendNotification);
        l.setFont(juce::Font(13.0f));
        l.setColour(juce::Label::textColourId, kDim);
        addAndMakeVisible(l);
    };
    rowCaption(pcSetLabel_, "PC SET");
    rowCaption(forteLabel_, "Forte");
    rowCaption(patternLabel_, "PATTERN");
    rowCaption(subdivLabel_, "Subdiv");
    rowCaption(repeatLabel_, "Repeat");
    rowCaption(scaleLabel_, "SCALE");

    noteDisplayLabel_.setText("no notes", juce::dontSendNotification);
    noteDisplayLabel_.setFont(juce::Font(15.0f));
    noteDisplayLabel_.setColour(juce::Label::textColourId, kText);
    noteDisplayLabel_.setColour(juce::Label::backgroundColourId, kPanel);
    addAndMakeVisible(noteDisplayLabel_);

    // Action buttons.
    for (int i = 0; i < 6; ++i) {
        styleButton(actionButtons_[(size_t)i]);
        actionButtons_[(size_t)i].setButtonText(kActionNames[i]);
        actionButtons_[(size_t)i].onClick = [this, i] {
            processor_.setGridAction(i);
            refreshActionButtons();
            refreshGridButtons();
        };
        addAndMakeVisible(actionButtons_[(size_t)i]);
    }

    // Grid buttons.
    for (int i = 0; i < 12; ++i) {
        styleButton(gridButtons_[(size_t)i]);
        gridButtons_[(size_t)i].setButtonText(juce::String(i) + " " + kNoteNames[i]);
        gridButtons_[(size_t)i].onClick = [this, i] {
            processor_.gridButtonPressed(i);
            refreshGridButtons();
        };
        addAndMakeVisible(gridButtons_[(size_t)i]);
    }

    auto setupEditor = [&](juce::TextEditor& ed, const juce::String& tooltip) {
        ed.setColour(juce::TextEditor::backgroundColourId, kPanel);
        ed.setColour(juce::TextEditor::textColourId, kText);
        ed.setColour(juce::TextEditor::highlightColourId, kMagentaDark);
        ed.setFont(juce::Font(15.0f));
        ed.setTooltip(tooltip);
        addAndMakeVisible(ed);
    };
    setupEditor(pcSetEditor_, "PC set, space separated (0 3 7). Enter to apply.");
    setupEditor(forteEditor_, "Forte number, e.g. 6-31. Enter to apply.");
    setupEditor(patternEditor_, "Rhythm pattern: x note, - rest, _ sustain, R random, [] subdivide");
    pcSetEditor_.setText(processor_.pcSetText(), juce::dontSendNotification);
    forteEditor_.setText(processor_.forteText(), juce::dontSendNotification);
    patternEditor_.setText(processor_.patternText(), juce::dontSendNotification);
    pcSetEditor_.onReturnKey = [this] { commitPcSetText(); };
    pcSetEditor_.onFocusLost = [this] { commitPcSetText(); };
    forteEditor_.onReturnKey = [this] { commitForteText(); };
    forteEditor_.onFocusLost = [this] { commitForteText(); };
    patternEditor_.onReturnKey = [this] { commitPatternText(); };
    patternEditor_.onFocusLost = [this] { commitPatternText(); };

    auto setupCombo = [&](juce::ComboBox& cb, const juce::String& tooltip) {
        cb.setColour(juce::ComboBox::backgroundColourId, kPanel);
        cb.setColour(juce::ComboBox::textColourId, kText);
        cb.setTooltip(tooltip);
        addAndMakeVisible(cb);
    };
    setupCombo(rootCombo_, "Root pitch class");
    setupCombo(baseOctaveCombo_, "Base octave");
    setupCombo(scaleRootCombo_, "Scale root");
    setupCombo(scaleOctaveCombo_, "Scale octave");
    setupCombo(scaleCombo_, "Scale");
    setupCombo(subdivCombo_, "Subdivision");
    setupCombo(repeatCombo_, "Repeat count");
    for (int i = 0; i < 12; ++i) {
        rootCombo_.addItem(kNoteNames[i], i + 1);
        scaleRootCombo_.addItem(kNoteNames[i], i + 1);
    }
    for (int i = 0; i <= 10; ++i) {
        baseOctaveCombo_.addItem(juce::String(i), i + 1);
        scaleOctaveCombo_.addItem(juce::String(i), i + 1);
    }
    for (const auto& s : seqcalc::scaleLibrary()) scaleCombo_.addItem(s.name, scaleCombo_.getNumItems() + 1);
    const char* const subdivs[9] = {"1m", "2m", "3m", "4m", "1n", "2n", "4n", "8n", "16n"};
    for (int i = 0; i < 9; ++i) subdivCombo_.addItem(subdivs[i], i + 1);
    for (int i = 1; i <= 8; ++i) repeatCombo_.addItem(juce::String(i), i);

    auto setupToggle = [&](juce::ToggleButton& t, const juce::String& label,
                           const juce::String& tooltip) {
        styleToggle(t);
        t.setButtonText(label);
        t.setTooltip(tooltip);
        addAndMakeVisible(t);
    };
    setupToggle(shuffleToggle_, "Shuffle", "Shuffle the pattern order");
    setupToggle(randomInScaleToggle_, "R In Scale", "Random notes from the scale (off: outside the scale)");
    setupToggle(rootOnlyToggle_, "Root Only", "Pattern uses the root note only");
    setupToggle(restsForRToggle_, "Rests For R", "R becomes a random rest instead of a random note");
    setupToggle(usePcSetToggle_, "Use PC Set", "On: pitch-class set. Off: scale mode");

    // Transport + actions.
    styleButton(calcButton_);
    calcButton_.setButtonText("CalcS");
    calcButton_.setTooltip("Calculate the pattern from the current inputs");
    calcButton_.onClick = [this] {
        if (!processor_.calc()) {
            // Error is shown by the timer in the note display.
        }
        refreshGridButtons();
    };
    addAndMakeVisible(calcButton_);

    styleButton(sendButton_);
    sendButton_.setButtonText("SEND");
    sendButton_.setTooltip("Play the calculated pattern out of the MIDI output");
    sendButton_.onClick = [this] {
        if (auto* p = processor_.apvts.getParameter("play"))
            p->setValueNotifyingHost(1.0f);
    };
    addAndMakeVisible(sendButton_);

    styleButton(stopButton_);
    stopButton_.setButtonText("STOP");
    stopButton_.setTooltip("Stop playback (latched until Play, cleared by a DAW stop)");
    stopButton_.onClick = [this] {
        if (auto* p = processor_.apvts.getParameter("play"))
            p->setValueNotifyingHost(0.0f);
    };
    addAndMakeVisible(stopButton_);

    styleButton(clearSetButton_);
    clearSetButton_.setButtonText("clrS");
    clearSetButton_.setTooltip("Clear the Forte and PC-set fields");
    clearSetButton_.onClick = [this] {
        processor_.clearSet();
        pcSetEditor_.setText(processor_.pcSetText(), juce::dontSendNotification);
        forteEditor_.setText(processor_.forteText(), juce::dontSendNotification);
        refreshGridButtons();
    };
    addAndMakeVisible(clearSetButton_);

    styleButton(resetButton_);
    resetButton_.setButtonText("RESET");
    resetButton_.setTooltip("Restore the default clip string");
    resetButton_.onClick = [this] {
        processor_.resetToDefaults();
        pcSetEditor_.setText(processor_.pcSetText(), juce::dontSendNotification);
        forteEditor_.setText(processor_.forteText(), juce::dontSendNotification);
        patternEditor_.setText(processor_.patternText(), juce::dontSendNotification);
        refreshGridButtons();
        refreshActionButtons();
    };
    addAndMakeVisible(resetButton_);

    styleButton(dragHandle_);
    addAndMakeVisible(dragHandle_);

    bpmLabel_.setText("Tempo", juce::dontSendNotification);
    bpmLabel_.setFont(juce::Font(13.0f));
    bpmLabel_.setColour(juce::Label::textColourId, kDim);
    addAndMakeVisible(bpmLabel_);
    bpmSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    bpmSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    bpmSlider_.setColour(juce::Slider::thumbColourId, kMagenta);
    bpmSlider_.setColour(juce::Slider::trackColourId, kButton);
    bpmSlider_.setColour(juce::Slider::textBoxTextColourId, kText);
    bpmSlider_.setTooltip("Standalone tempo (the DAW tempo wins when the transport runs)");
    addAndMakeVisible(bpmSlider_);

    addAndMakeVisible(pianoStrip_);

    // APVTS attachments.
    auto& apvts = processor_.apvts;
    rootAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "root", rootCombo_);
    baseOctaveAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "baseOctave", baseOctaveCombo_);
    scaleRootAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "scaleRoot", scaleRootCombo_);
    scaleOctaveAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "scaleOctave", scaleOctaveCombo_);
    scaleAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "scale", scaleCombo_);
    subdivAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "subdiv", subdivCombo_);
    repeatAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "repeat", repeatCombo_);
    shuffleAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "shuffle", shuffleToggle_);
    randomInScaleAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "randomInScale", randomInScaleToggle_);
    rootOnlyAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "rootOnly", rootOnlyToggle_);
    restsForRAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "restsForR", restsForRToggle_);
    usePcSetAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "usePcSet", usePcSetToggle_);
    bpmAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "bpm", bpmSlider_);

    refreshActionButtons();
    refreshGridButtons();
    startTimerHz(30);

    // setSize() LAST, after every child component exists.
    setSize(980, 500);
}

void SeqCalcEditor::paint(juce::Graphics& g) {
    g.fillAll(kBg);
}

void SeqCalcEditor::resized() {
    const int W = getWidth();
    titleLabel_.setBounds(16, 10, 300, 28);
    creditLabel_.setBounds(W - 480, 14, 464, 22);
    staleLabel_.setBounds(W - 320, 42, 304, 24);

    // Action buttons.
    int x = 16;
    for (int i = 0; i < 6; ++i) {
        actionButtons_[(size_t)i].setBounds(x, 42, 100, 26);
        x += 108;
    }
    // Grid.
    x = 16;
    for (int i = 0; i < 12; ++i) {
        gridButtons_[(size_t)i].setBounds(x, 78, 52, 40);
        x += 56;
    }
    // PC-set / Forte row.
    pcSetLabel_.setBounds(16, 128, 70, 28);
    pcSetEditor_.setBounds(90, 128, 300, 28);
    forteLabel_.setBounds(402, 128, 64, 28);
    forteEditor_.setBounds(470, 128, 130, 28);
    clearSetButton_.setBounds(612, 128, 64, 28);
    // Pattern row.
    patternLabel_.setBounds(16, 164, 70, 28);
    patternEditor_.setBounds(90, 164, 300, 28);
    subdivLabel_.setBounds(402, 164, 64, 28);
    subdivCombo_.setBounds(470, 164, 80, 28);
    repeatLabel_.setBounds(556, 164, 52, 28);
    repeatCombo_.setBounds(612, 164, 64, 28);
    // Scale row.
    scaleLabel_.setBounds(16, 200, 70, 28);
    rootCombo_.setBounds(90, 200, 70, 28);
    baseOctaveCombo_.setBounds(168, 200, 60, 28);
    scaleRootCombo_.setBounds(236, 200, 70, 28);
    scaleOctaveCombo_.setBounds(314, 200, 60, 28);
    scaleCombo_.setBounds(382, 200, 180, 28);
    usePcSetToggle_.setBounds(574, 200, 110, 28);
    // Toggles row.
    x = 16;
    juce::ToggleButton* toggles[4] = {&shuffleToggle_, &randomInScaleToggle_,
                                      &rootOnlyToggle_, &restsForRToggle_};
    const int toggleWidths[4] = {100, 130, 110, 130};
    for (int i = 0; i < 4; ++i) {
        toggles[i]->setBounds(x, 236, toggleWidths[i], 26);
        x += toggleWidths[i] + 12;
    }
    // Note display.
    noteDisplayLabel_.setBounds(16, 272, W - 32, 30);
    // Piano strip.
    pianoStrip_.setBounds(16, 310, W - 32, 64);
    // Transport row.
    calcButton_.setBounds(16, 390, 120, 40);
    sendButton_.setBounds(148, 390, 120, 40);
    stopButton_.setBounds(280, 390, 120, 40);
    resetButton_.setBounds(412, 390, 80, 40);
    dragHandle_.setBounds(504, 390, 200, 40);
    bpmLabel_.setBounds(716, 390, 50, 40);
    bpmSlider_.setBounds(766, 390, W - 782, 40);
}

void SeqCalcEditor::timerCallback() {
    const std::string display = processor_.noteDisplay();
    const std::string error = processor_.calcError();
    const bool stale = processor_.isStale();
    const bool playing = processor_.isEffectivelyPlaying();
    const auto notes = processor_.calculatedNotes();

    if (display != lastDisplay_ || error != lastError_) {
        lastDisplay_ = display;
        lastError_ = error;
        if (!error.empty()) {
            noteDisplayLabel_.setText(error, juce::dontSendNotification);
            noteDisplayLabel_.setColour(juce::Label::textColourId, kMagenta);
        } else {
            noteDisplayLabel_.setText(display.empty() ? "no notes"
                                                      : display,
                                      juce::dontSendNotification);
            noteDisplayLabel_.setColour(juce::Label::textColourId, kText);
        }
    }
    if (stale != lastStale_) {
        lastStale_ = stale;
        staleLabel_.setVisible(stale);
    }
    if (playing != lastPlaying_) {
        lastPlaying_ = playing;
        sendButton_.setColour(juce::TextButton::buttonColourId,
                              playing ? kMagentaDark : kButton);
    }
    if (notes != lastNotes_) {
        lastNotes_ = notes;
        pianoStrip_.setNotes(notes);
        refreshGridButtons();
    }
}

void SeqCalcEditor::refreshActionButtons() {
    const int active = processor_.gridAction();
    for (int i = 0; i < 6; ++i) {
        actionButtons_[(size_t)i].setColour(
            juce::TextButton::buttonColourId,
            i == active ? kMagentaDark : kButton);
    }
}

void SeqCalcEditor::refreshGridButtons() {
    const int action = processor_.gridAction();
    const auto pcSet = processor_.pcSet();
    auto getInt = [&](const char* id, int fallback) {
        if (auto* p = dynamic_cast<juce::AudioParameterInt*>(
                processor_.apvts.getParameter(id)))
            return p->get();
        return fallback;
    };
    auto getChoice = [&](const char* id, int fallback) {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(
                processor_.apvts.getParameter(id)))
            return p->getIndex();
        return fallback;
    };
    for (int i = 0; i < 12; ++i) {
        bool active = false;
        switch (action) {
            case 0: active = getChoice("root", 0) == i; break;
            case 1: active = getInt("baseOctave", 4) == i; break;
            case 2: active = getInt("octaveRange", 1) == i; break;
            case 3: active = std::find(pcSet.begin(), pcSet.end(), i) != pcSet.end(); break;
            default: break;
        }
        gridButtons_[(size_t)i].setColour(
            juce::TextButton::buttonColourId,
            active ? kMagentaDark : kButton);
    }
}

void SeqCalcEditor::commitPcSetText() {
    processor_.setPcSetText(pcSetEditor_.getText().toStdString());
    pcSetEditor_.setText(processor_.pcSetText(), juce::dontSendNotification);
    refreshGridButtons();
}

void SeqCalcEditor::commitForteText() {
    processor_.setForteText(forteEditor_.getText().toStdString());
}

void SeqCalcEditor::commitPatternText() {
    processor_.setPatternText(patternEditor_.getText().toStdString());
}
