// SeqCalc VST v0.1.0 — JUCE VST3 port of Steve Zydek's SeqCalc Max for
// Live sequencer/calculator. Design: docs in VST-PORT-DRAFT.md.
// MIDI-generating plugin: Calc renders the pattern, Send plays it out
// of the MIDI output (live) and the drag handle exports it as a MIDI
// file. No audio input; stereo output is silent.
// IS_SYNTH with NEEDS_MIDI_INPUT TRUE (Ableton rejects a synth
// without it) and NEEDS_MIDI_OUTPUT TRUE (MIDI out is the product).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "../core/ClipRenderer.h"
#include "../core/ForteTable.h"

class SeqCalcProcessor : public juce::AudioProcessor,
                         public juce::AudioProcessorValueTreeState::Listener {
public:
    SeqCalcProcessor();
    ~SeqCalcProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SeqCalc"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ---- engine API (message thread; audio thread reads snapshots) ----
    // Calc: run the §2.6 pipeline on the current inputs and swap in the
    // rendered pattern. Atomic: Send always sees a fully calculated
    // pattern, never a half-calculated one. Returns false with
    // calcError() set when the Forte number is unknown (old pattern kept).
    bool calc();
    std::string calcError() const;
    bool isStale() const { return stale_.load(); }
    void markStale() { stale_ = true; }
    // The calculated note display ("C4 D4 F4"), empty before first Calc.
    std::string noteDisplay() const;
    // MIDI notes of the calculated note list (for the piano strip).
    std::vector<int> calculatedNotes() const;
    double patternLengthBeats() const;

    // Editor strings (saved in plugin state). The PC-set text is the
    // ordered set; the 12 PC toggles reflect membership.
    void setPcSetText(const std::string& text);
    std::string pcSetText() const;
    std::vector<int> pcSet() const; // ordered pitch classes
    void setForteText(const std::string& text);
    std::string forteText() const;
    void setPatternText(const std::string& text);
    std::string patternText() const;

    // Grid (message thread). The action buttons choose which parameter
    // the 12 grid buttons edit (params.js actions 0-5).
    int gridAction() const { return gridAction_; }
    void setGridAction(int action); // 0 Root, 1 Roct, 2 Range, 3 PC, 4 Rhy, 5 Forte
    void gridButtonPressed(int pcIndex); // 0..11

    // clrS: clear the Forte + PC-set fields (and the piano display).
    void clearSet();
    // RESET: restore the default clip string (draft §2.4).
    void resetToDefaults();

    // Playback. setPlaying follows the Pitchulator pattern: flag-only,
    // processBlock consumes the request on the audio thread.
    void setPlaying(bool on);
    bool isPlaying() const { return playing_; }
    bool isHostPlaying() const { return hostPlaying_; }
    bool isLatchedStopped() const { return latchedStopped_; }
    bool isEffectivelyPlaying() const {
        return playing_ || (hostPlaying_ && !latchedStopped_);
    }
    double hostBpm() const { return hostBpm_; }
    double bpm() const;
    double effectiveBpm() const;

    // Drag export: write the current rendered pattern to a MIDI file
    // in the temp directory and return it (empty file on failure).
    // The file is deleted first, then written with truncate, so a
    // stale export can never survive.
    juce::File exportMidiFile();

    // AudioProcessorValueTreeState::Listener
    void parameterChanged(const juce::String& parameterID, float newValue) override;

private:
    void setChoiceParam(const juce::String& id, int index);
    void setIntParam(const juce::String& id, int value);
    void setBoolParam(const juce::String& id, bool value);
    int getChoiceParam(const juce::String& id, int fallback) const;
    int getIntParam(const juce::String& id, int fallback) const;
    bool getBoolParam(const juce::String& id, bool fallback) const;
    void syncPcTogglesFromText(); // toggles follow pcSetText membership
    void allNotesOff(juce::MidiBuffer& midiMessages, int sampleOffset);

    // Double-buffered calculated pattern: the message thread writes the
    // inactive slot and flips activeSlot_; the audio thread only reads.
    std::array<std::shared_ptr<const seqcalc::CalcOutput>, 2> slots_;
    std::atomic<int> activeSlot_{0};
    mutable std::mutex calcMutex_; // guards calcError_, noteDisplay_, calculatedNotes_
    std::string calcError_;
    std::string noteDisplay_;
    std::vector<int> calculatedNotes_;
    std::atomic<bool> stale_{true};

    std::string pcSetText_ = "0 3 7";
    std::string forteText_;
    std::string patternText_ = "xxxx";
    int gridAction_ = 3; // PC

    // Sequencer state (audio thread only, except the atomics).
    std::atomic<bool> playing_{false};
    std::atomic<bool> startRequested_{false};
    std::atomic<bool> hostPlaying_{false};
    std::atomic<double> hostBpm_{0.0};
    std::atomic<bool> latchedStopped_{false};
    double sampleRate_ = 44100.0;
    double lastHostPpq_ = -1.0;
    double anchorPpq_ = 0.0;       // ppq where the current run started
    const seqcalc::CalcOutput* lastPattern_ = nullptr;
    // Sounding notes (MIDI note numbers). Normal note-offs fire from
    // the scheduler; this set exists so a stop silences everything.
    std::set<int> soundingNotes_;
    double runBeats_ = 0.0; // beats since the current run started

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SeqCalcProcessor)
};
