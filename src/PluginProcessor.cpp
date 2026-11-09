// SeqCalc VST v0.1.0 — processor. See PluginProcessor.h and
// VST-PORT-DRAFT.md for the design.
#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>

#include "PluginEditor.h"
#include "../core/PatternEngine.h"
#include "../core/ScaleLibrary.h"
#include "../core/SetTheory.h"

namespace {

const char* const kNoteNames[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

const char* const kSubdivNames[9] = {
    "1m", "2m", "3m", "4m", "1n", "2n", "4n", "8n", "16n"
};

const char* const kRhySet[12] = {
    "x", "R", "-", "_", "[", "]", "", "", "", "", "", ""
};

const char* const kForteSet[12] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "Z", "-"
};

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

} // namespace

SeqCalcProcessor::SeqCalcProcessor()
    : AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "SeqCalcParameters", createParameterLayout()) {
    // JUCE 8: APVTS listeners are per-parameter (the old catch-all
    // addListener is gone). Register this processor for every parameter.
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
            apvts.addParameterListener(ranged->getParameterID(), this);
    slots_[0] = std::make_shared<const seqcalc::CalcOutput>();
    slots_[1] = std::make_shared<const seqcalc::CalcOutput>();
    syncPcTogglesFromText();
    calc(); // always have a calculated pattern, even before the first CalcS
}

SeqCalcProcessor::~SeqCalcProcessor() {
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
            apvts.removeParameterListener(ranged->getParameterID(), this);
}

juce::AudioProcessorValueTreeState::ParameterLayout
SeqCalcProcessor::createParameterLayout() {
    juce::StringArray noteNames;
    for (auto* n : kNoteNames) noteNames.add(n);
    juce::StringArray subdivNames;
    for (auto* s : kSubdivNames) subdivNames.add(s);
    juce::StringArray scaleNames;
    for (const auto& s : seqcalc::scaleLibrary()) scaleNames.add(s.name);

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "root", "Root", noteNames, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "baseOctave", "Base Octave", 0, 10, 4));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "octaveRange", "Octave Range", 1, 8, 1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "scaleRoot", "Scale Root", noteNames, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "scaleOctave", "Scale Octave", 0, 10, 3));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "scale", "Scale", scaleNames, 0));
    // PC set: minor triad 0, 3, 7 by default.
    const int defaultPcs[3] = {0, 3, 7};
    for (int i = 0; i < 12; ++i) {
        const bool on = std::find(std::begin(defaultPcs), std::end(defaultPcs), i)
                        != std::end(defaultPcs);
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            "pc" + juce::String(i), juce::String("PC ") + juce::String(i), on));
    }
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "subdiv", "Subdivision", subdivNames, 6)); // 4n
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "repeat", "Repeat", 1, 8, 1));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "shuffle", "Shuffle", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "randomInScale", "Random In Scale", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "rootOnly", "Root Only", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "restsForR", "Rests For R", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "usePcSet", "Use PC Set", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "play", "Play", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "bpm", "Tempo", juce::NormalisableRange<float>(40.0f, 240.0f, 1.0f),
        120.0f));
    return {params.begin(), params.end()};
}

// ---------------------------------------------------------------- state
void SeqCalcProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    juce::ValueTree extra("SeqCalcData");
    extra.setProperty("pcSetText", juce::String(pcSetText_), nullptr);
    extra.setProperty("forteText", juce::String(forteText_), nullptr);
    extra.setProperty("patternText", juce::String(patternText_), nullptr);
    extra.setProperty("gridAction", gridAction_, nullptr);
    state.appendChild(extra, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SeqCalcProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml == nullptr || !xml->hasTagName(apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml(*xml);
    auto extra = state.getChildWithName("SeqCalcData");
    if (extra.isValid()) {
        pcSetText_ = extra.getProperty("pcSetText", "0 3 7").toString().toStdString();
        forteText_ = extra.getProperty("forteText", "").toString().toStdString();
        patternText_ = extra.getProperty("patternText", "xxxx").toString().toStdString();
        gridAction_ = (int)extra.getProperty("gridAction", 3);
        state.removeChild(extra, nullptr);
    }
    apvts.replaceState(state);
    syncPcTogglesFromText();
    calc();
}

// ------------------------------------------------------------- parameters
int SeqCalcProcessor::getChoiceParam(const juce::String& id, int fallback) const {
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id)))
        return p->getIndex();
    return fallback;
}

int SeqCalcProcessor::getIntParam(const juce::String& id, int fallback) const {
    if (auto* p = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter(id)))
        return p->get();
    return fallback;
}

bool SeqCalcProcessor::getBoolParam(const juce::String& id, bool fallback) const {
    if (auto* p = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(id)))
        return p->get();
    return fallback;
}

void SeqCalcProcessor::setChoiceParam(const juce::String& id, int index) {
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id))) {
        const int count = p->choices.size();
        const int clamped = std::max(0, std::min(count - 1, index));
        p->setValueNotifyingHost((float)clamped / (float)std::max(1, count - 1));
    }
}

void SeqCalcProcessor::setIntParam(const juce::String& id, int value) {
    if (auto* p = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter(id))) {
        const int clamped = std::max(p->getRange().getStart(),
                                    std::min(p->getRange().getEnd(), value));
        p->setValueNotifyingHost(p->convertTo0to1((float)clamped));
    }
}

void SeqCalcProcessor::setBoolParam(const juce::String& id, bool value) {
    if (auto* p = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(id)))
        p->setValueNotifyingHost(value ? 1.0f : 0.0f);
}

void SeqCalcProcessor::parameterChanged(const juce::String& parameterID,
                                        float newValue) {
    if (parameterID == "play") {
        setPlaying(newValue > 0.5f);
        return;
    }
    // Any other parameter change makes the current calculation stale.
    stale_ = true;
}

bool SeqCalcProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

// ------------------------------------------------------------------- calc
bool SeqCalcProcessor::calc() {
    seqcalc::CalcInputs in;
    in.rootPc = getChoiceParam("root", 0);
    in.baseOctave = getIntParam("baseOctave", 4);
    in.octaveRange = getIntParam("octaveRange", 1);
    in.usePcSet = getBoolParam("usePcSet", true);
    in.pcSet = seqcalc::parsePcSet(pcSetText_);
    in.forteNumber = trim(forteText_);
    in.scaleRootPc = getChoiceParam("scaleRoot", 0);
    in.scaleOctave = getIntParam("scaleOctave", 3);
    in.scaleIndex = getChoiceParam("scale", 0);
    in.pattern = patternText_;
    in.subdiv = kSubdivNames[std::max(0, std::min(8, getChoiceParam("subdiv", 6)))];
    in.repeat = getIntParam("repeat", 1);
    in.shuffle = getBoolParam("shuffle", false);
    in.randomInScale = getBoolParam("randomInScale", true);
    in.rootOnly = getBoolParam("rootOnly", false);
    in.restsForR = getBoolParam("restsForR", false);
    in.randomSeed = 0; // live use is nondeterministic; tests pass a seed

    auto out = std::make_shared<const seqcalc::CalcOutput>(seqcalc::renderClip(in));
    if (!out->valid) {
        std::lock_guard<std::mutex> lock(calcMutex_);
        calcError_ = out->error;
        return false;
    }

    // One-shot Forte: a successful lookup fills the PC set with the
    // prime form, then the Forte field clears so a later edit to the
    // set is not silently overwritten by the next Calc.
    if (!out->forteUsed.empty()) {
        pcSetText_ = seqcalc::formatPcSet(out->fortePrimeForm);
        forteText_.clear();
        syncPcTogglesFromText();
    }

    {
        std::lock_guard<std::mutex> lock(calcMutex_);
        calcError_.clear();
        noteDisplay_ = out->displayText;
        calculatedNotes_ = out->noteList;
    }
    const int inactive = 1 - activeSlot_.load();
    slots_[(size_t)inactive] = out;
    activeSlot_.store(inactive);
    stale_ = false;
    return true;
}

std::string SeqCalcProcessor::calcError() const {
    std::lock_guard<std::mutex> lock(calcMutex_);
    return calcError_;
}

std::string SeqCalcProcessor::noteDisplay() const {
    std::lock_guard<std::mutex> lock(calcMutex_);
    return noteDisplay_;
}

std::vector<int> SeqCalcProcessor::calculatedNotes() const {
    std::lock_guard<std::mutex> lock(calcMutex_);
    return calculatedNotes_;
}

double SeqCalcProcessor::patternLengthBeats() const {
    const auto* pattern = slots_[(size_t)activeSlot_.load()].get();
    return pattern != nullptr ? pattern->lengthBeats : 0.0;
}

// ------------------------------------------------------------- editor text
void SeqCalcProcessor::setPcSetText(const std::string& text) {
    pcSetText_ = text;
    syncPcTogglesFromText();
    markStale();
}

std::string SeqCalcProcessor::pcSetText() const { return pcSetText_; }

std::vector<int> SeqCalcProcessor::pcSet() const {
    return seqcalc::parsePcSet(pcSetText_);
}

void SeqCalcProcessor::setForteText(const std::string& text) {
    forteText_ = text;
    markStale();
}

std::string SeqCalcProcessor::forteText() const { return forteText_; }

void SeqCalcProcessor::setPatternText(const std::string& text) {
    patternText_ = text;
    markStale();
}

std::string SeqCalcProcessor::patternText() const { return patternText_; }

void SeqCalcProcessor::syncPcTogglesFromText() {
    const auto set = seqcalc::parsePcSet(pcSetText_);
    for (int i = 0; i < 12; ++i) {
        const bool on = std::find(set.begin(), set.end(), i) != set.end();
        setBoolParam("pc" + juce::String(i), on);
    }
}

// ------------------------------------------------------------------- grid
void SeqCalcProcessor::setGridAction(int action) {
    gridAction_ = std::max(0, std::min(5, action));
}

void SeqCalcProcessor::gridButtonPressed(int pcIndex) {
    if (pcIndex < 0 || pcIndex > 11) return;
    switch (gridAction_) {
        case 0: // Root
            setChoiceParam("root", pcIndex);
            break;
        case 1: // Roct
            setIntParam("baseOctave", std::min(pcIndex, 10));
            break;
        case 2: // Range
            setIntParam("octaveRange", std::max(1, std::min(pcIndex, 8)));
            break;
        case 3: { // PC: toggle this pitch class in the ordered set
            auto set = seqcalc::parsePcSet(pcSetText_);
            auto it = std::find(set.begin(), set.end(), pcIndex);
            if (it != set.end())
                set.erase(it);
            else
                set.push_back(pcIndex);
            pcSetText_ = seqcalc::formatPcSet(set);
            syncPcTogglesFromText();
            markStale();
            break;
        }
        case 4: { // Rhy: append the rhythm char (params.js rhyset)
            if (pcIndex < 6 && kRhySet[pcIndex][0] != '\0') {
                patternText_ += kRhySet[pcIndex];
                markStale();
            }
            break;
        }
        case 5: // Forte: append the Forte char (params.js fset)
            forteText_ += kForteSet[pcIndex];
            markStale();
            break;
        default:
            break;
    }
}

void SeqCalcProcessor::clearSet() {
    forteText_.clear();
    pcSetText_.clear();
    syncPcTogglesFromText();
    markStale();
    calc();
}

void SeqCalcProcessor::resetToDefaults() {
    setChoiceParam("root", 0);
    setIntParam("baseOctave", 4);
    setIntParam("octaveRange", 1);
    setChoiceParam("scaleRoot", 0);
    setIntParam("scaleOctave", 3);
    setChoiceParam("scale", 0);
    pcSetText_ = "0 3 7";
    forteText_.clear();
    patternText_ = "xxxx";
    syncPcTogglesFromText();
    setChoiceParam("subdiv", 6);
    setIntParam("repeat", 1);
    setBoolParam("shuffle", false);
    setBoolParam("randomInScale", true);
    setBoolParam("rootOnly", false);
    setBoolParam("restsForR", false);
    setBoolParam("usePcSet", true);
    gridAction_ = 3;
    markStale();
    calc();
}

// --------------------------------------------------------------- playback
void SeqCalcProcessor::setPlaying(bool on) {
    playing_ = on;
    startRequested_ = on;
    if (on)
        latchedStopped_ = false;
    else if (hostPlaying_)
        latchedStopped_ = true;
}

double SeqCalcProcessor::bpm() const {
    if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("bpm")))
        return (double)p->get();
    return 120.0;
}

double SeqCalcProcessor::effectiveBpm() const {
    const double hb = hostBpm_.load();
    if (hostPlaying_ && hb > 0.0 && std::isfinite(hb)) return hb;
    return bpm();
}

void SeqCalcProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::ignoreUnused(samplesPerBlock);
    sampleRate_ = sampleRate;
}

void SeqCalcProcessor::allNotesOff(juce::MidiBuffer& midiMessages, int sampleOffset) {
    for (int note : soundingNotes_)
        midiMessages.addEvent(juce::MidiMessage::noteOff(1, note), sampleOffset);
    soundingNotes_.clear();
}

void SeqCalcProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // This plugin does not respond to incoming MIDI: consume it so
    // host notes never pass through to the output.
    midiMessages.clear();

    // --- transport ---
    bool hostPlayingNow = false;
    double hostBpmValue = 0.0;
    juce::Optional<double> hostPpq;
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            hostPlayingNow = pos->getIsPlaying();
            if (auto b = pos->getBpm()) hostBpmValue = *b;
            if (auto ppq = pos->getPpqPosition()) hostPpq = ppq;
        }
    }
    const bool wasHostPlaying = hostPlaying_.exchange(hostPlayingNow);
    if (hostBpmValue > 0.0 && std::isfinite(hostBpmValue)) hostBpm_ = hostBpmValue;

    const bool latched = latchedStopped_.load();
    const bool hostStart = hostPlayingNow && !wasHostPlaying && !latched;
    bool loopBack = false;
    if (hostPlayingNow && !latched && hostPpq && lastHostPpq_ >= 0.0
        && *hostPpq < lastHostPpq_ - 0.01)
        loopBack = true;
    if (hostPlayingNow && hostPpq) lastHostPpq_ = *hostPpq;

    if (!hostPlayingNow && wasHostPlaying) {
        // DAW transport stop: stop playback and silence everything.
        // (Also clears the VST-Stop latch, so the next DAW start is fresh.)
        playing_ = false;
        startRequested_ = false;
        latchedStopped_ = false;
        allNotesOff(midiMessages, 0);
        lastHostPpq_ = -1.0;
        runBeats_ = 0.0;
    }

    const bool internalStart = startRequested_.exchange(false);
    if (internalStart || hostStart || loopBack) {
        // (Re)start at step 1.
        runBeats_ = 0.0;
        if (hostPpq) anchorPpq_ = *hostPpq;
        allNotesOff(midiMessages, 0);
    }

    // A fresh Calc swaps the pattern: restart from step 1.
    const auto* pattern = slots_[(size_t)activeSlot_.load()].get();
    if (pattern != lastPattern_) {
        lastPattern_ = pattern;
        runBeats_ = 0.0;
        allNotesOff(midiMessages, 0);
    }

    const bool running = (hostPlayingNow && !latched) || playing_;
    if (!running || pattern == nullptr || !pattern->valid
        || pattern->lengthBeats <= 0.0 || pattern->events.empty()) {
        if (!soundingNotes_.empty()) allNotesOff(midiMessages, 0);
        return;
    }

    // --- scheduling window, in beats since the run started ---
    const double L = pattern->lengthBeats;
    const double blockBeats =
        (double)numSamples / sampleRate_ * effectiveBpm() / 60.0;
    double newRunBeats;
    if (hostPlayingNow && !latched && hostPpq)
        newRunBeats = *hostPpq - anchorPpq_;
    else
        newRunBeats = runBeats_ + blockBeats;
    if (newRunBeats < runBeats_) newRunBeats = runBeats_; // host seek guard
    const double delta = newRunBeats - runBeats_;
    if (delta <= 0.0) {
        runBeats_ = newRunBeats;
        return;
    }

    // Fire every occurrence of a pattern-time in the window, so short
    // patterns looping more than once per block still hit every step.
    auto fireInWindow = [&](double patternTime, auto&& emit) {
        double t = patternTime
                   + std::floor((runBeats_ - patternTime) / L) * L;
        if (t < runBeats_) t += L;
        int guard = 0;
        while (t < runBeats_ + delta && guard++ < 64) {
            const int offset = (int)((t - runBeats_) / delta * numSamples);
            emit(std::max(0, std::min(numSamples - 1, offset)));
            t += L;
        }
    };

    // Note-offs first, then note-ons: at a loop boundary the last
    // note of the old loop and the first note of the new loop can be
    // the same pitch at the same time, and the off must land first
    // or it kills the fresh note-on.
    // A note-off only fires for a note that is actually sounding:
    // an end time of exactly the pattern length maps to t=0 in the
    // first block of a run, before its note has ever sounded, and
    // that spurious off must not fire.
    for (const auto& e : pattern->events) {
        fireInWindow(e.startBeats + e.durationBeats, [&](int offset) {
            if (soundingNotes_.count(e.midiNote) == 0) return;
            midiMessages.addEvent(juce::MidiMessage::noteOff(1, e.midiNote), offset);
            soundingNotes_.erase(e.midiNote);
        });
    }
    for (const auto& e : pattern->events) {
        fireInWindow(e.startBeats, [&](int offset) {
            if (soundingNotes_.count(e.midiNote) != 0) {
                // Retrigger: silence the ringing note first.
                midiMessages.addEvent(juce::MidiMessage::noteOff(1, e.midiNote), offset);
                soundingNotes_.erase(e.midiNote);
            }
            midiMessages.addEvent(
                juce::MidiMessage::noteOn(1, e.midiNote, (juce::uint8)e.velocity),
                offset);
            soundingNotes_.insert(e.midiNote);
        });
    }

    runBeats_ = newRunBeats;
}

// ------------------------------------------------------------ drag export
juce::File SeqCalcProcessor::exportMidiFile() {
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::File file = dir.getChildFile("SeqCalc-pattern.mid");
    file.deleteFile(); // never ship a stale export

    const auto* pattern = slots_[(size_t)activeSlot_.load()].get();
    if (pattern == nullptr || !pattern->valid || pattern->events.empty())
        return {};

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote(480);
    juce::MidiMessageSequence track;
    track.addEvent(juce::MidiMessage::tempoMetaEvent(
                       (int)(60000000.0 / effectiveBpm())),
                   0.0);
    for (const auto& e : pattern->events) {
        track.addEvent(juce::MidiMessage::noteOn(1, e.midiNote,
                                                 (juce::uint8)e.velocity),
                       e.startBeats * 480.0);
        track.addEvent(juce::MidiMessage::noteOff(1, e.midiNote),
                       (e.startBeats + e.durationBeats) * 480.0);
    }
    track.addEvent(juce::MidiMessage::endOfTrack(),
                   pattern->lengthBeats * 480.0);
    track.updateMatchedPairs();
    mf.addTrack(track);

    juce::FileOutputStream out(file);
    if (!out.openedOk()) return {};
    out.setPosition(0);
    out.truncate();
    mf.writeTo(out);
    out.flush();
    return file;
}

// ------------------------------------------------------------------ editor
juce::AudioProcessorEditor* SeqCalcProcessor::createEditor() {
    return new SeqCalcEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SeqCalcProcessor();
}
