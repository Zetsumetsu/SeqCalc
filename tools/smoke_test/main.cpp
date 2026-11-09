// SeqCalc smoke test: headless, no DAW, no GUI. Instantiates the
// processor, runs Calc on known inputs, and verifies the rendered
// events, the Forte corrections, the Calc->Send MIDI-out flow, and
// the drag-export MIDI file. Exit 0 on pass, 1 on failure.
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "../src/PluginProcessor.h"
#include "../core/ClipRenderer.h"
#include "../core/ForteTable.h"
#include "../core/PatternEngine.h"
#include "../core/SetTheory.h"

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(cond)) {                                                         \
            ++g_failures;                                                      \
            std::printf("FAIL: " __VA_ARGS__);                                 \
            std::printf("\n");                                                 \
        }                                                                      \
    } while (0)

static bool vecEq(const std::vector<int>& a, const std::vector<int>& b) {
    return a == b;
}

static void testForteTable() {
    const seqcalc::ForteTable& t = seqcalc::ForteTable::instance();
    CHECK(seqcalc::ForteTable::kRowCount == 206, "Forte table has 206 rows");
    CHECK(t.validate().empty(), "Forte table passes §3.3 validation");

    // Six §3.1 fixes.
    auto r = t.lookup("5-13");
    CHECK(r.has_value() && r->intervalVector == "221311", "5-13 iv 221311");
    r = t.lookup("6-2");
    CHECK(r.has_value() && r->intervalVector == "443211", "6-2 iv 443211");
    r = t.lookup("6-31");
    CHECK(r.has_value() && vecEq(r->primeForm, {0, 1, 3, 5, 8, 9})
              && r->intervalVector == "223431", "6-31 pf 013589 iv 223431");
    r = t.lookup("6-35");
    CHECK(r.has_value() && vecEq(r->primeForm, {0, 2, 4, 6, 8, 10}),
          "6-35 pf 0246810");
    r = t.lookup("6-Z12");
    CHECK(r.has_value() && vecEq(r->primeForm, {0, 1, 2, 4, 6, 7})
              && r->intervalVector == "332232",
          "6-Z12 pf 012467 iv 332232 (NOT 333231)");
    r = t.lookup("6-Z43");
    CHECK(r.has_value() && r->intervalVector == "322332", "6-Z43 iv 322332");

    // Six formerly-Rahn rows: Forte primes throughout.
    r = t.lookup("5-20");
    CHECK(r.has_value() && vecEq(r->primeForm, {0, 1, 3, 7, 8}), "5-20 Forte pf");
    r = t.lookup("6-Z29");
    CHECK(r.has_value() && vecEq(r->primeForm, {0, 1, 3, 6, 8, 9}), "6-Z29 Forte pf");
    r = t.lookup("7-Z18");
    CHECK(r.has_value() && vecEq(r->primeForm, {0, 1, 2, 3, 5, 8, 9}), "7-Z18 Forte pf");
    r = t.lookup("7-20");
    CHECK(r.has_value() && vecEq(r->primeForm, {0, 1, 2, 4, 7, 8, 9}), "7-20 Forte pf");
    r = t.lookup("8-26");
    CHECK(r.has_value() && vecEq(r->primeForm, {0, 1, 2, 4, 5, 7, 9, 10}), "8-26 Forte pf");

    // Lookup is case-insensitive on the Z, and trims whitespace.
    r = t.lookup(" 6-z12 ");
    CHECK(r.has_value(), "lookup trims whitespace and accepts lowercase z");
    CHECK(!t.lookup("9-99").has_value(), "unknown Forte number returns empty");
    CHECK(!t.lookup("").has_value(), "empty Forte number returns empty");

    // Interval-vector recompute agrees with the table everywhere.
    bool allIv = true;
    for (int i = 0; i < seqcalc::ForteTable::kRowCount; ++i) {
        auto row = t.rowAt(i);
        if (!row.has_value()
            || seqcalc::ForteTable::intervalVectorFor(row->primeForm)
                   != row->intervalVector)
            allIv = false;
    }
    CHECK(allIv, "every row's iv recomputes from its prime form");
}

static void testSetTheory() {
    auto notes = seqcalc::pcSetToMidi(0, 4, 1, {0, 2, 5});
    CHECK(vecEq(notes, {60, 62, 65}), "C4 0 2 5 -> C4 D4 F4 (60 62 65)");
    // Order is preserved: a typed 5 2 0 gives a different order.
    notes = seqcalc::pcSetToMidi(0, 4, 1, {5, 2, 0});
    CHECK(vecEq(notes, {65, 62, 60}), "order preserved: 5 2 0 -> F4 D4 C4");
    // Octave range: two octaves of a triad.
    notes = seqcalc::pcSetToMidi(0, 4, 2, {0, 4, 7});
    CHECK(vecEq(notes, {60, 64, 67, 72, 76, 79}), "2 octaves of C major triad");
    // Root B (11) with the first set note in the base octave:
    // settheory.js puts every note in the base octave (possibly
    // below the root): B4 for pc 0, D4 for pc 3.
    notes = seqcalc::pcSetToMidi(11, 4, 1, {0, 3});
    CHECK(vecEq(notes, {71, 62}), "root B4: pc 0 -> B4 (71), pc 3 -> D4 (62)");
    CHECK(seqcalc::midiToName(60) == "C4", "midiToName 60 = C4");
    CHECK(seqcalc::midiToName(61) == "C#4", "midiToName 61 = C#4");
    CHECK(seqcalc::midiToName(59) == "B3", "midiToName 59 = B3");
    CHECK(seqcalc::midiToName(21) == "A0", "midiToName 21 = A0");
    auto parsed = seqcalc::parsePcSet("0 3 7");
    CHECK(vecEq(parsed, {0, 3, 7}), "parsePcSet space separated");
    parsed = seqcalc::parsePcSet("0,3,7");
    CHECK(vecEq(parsed, {0, 3, 7}), "parsePcSet comma separated");
    // Single characters, like settheory.js: 'a' is 10, 'b' is 11,
    // and "13" is the two pitch classes 1 and 3.
    parsed = seqcalc::parsePcSet("0 3 13 7");
    CHECK(vecEq(parsed, {0, 3, 1, 3, 7}), "parsePcSet reads single characters");
    parsed = seqcalc::parsePcSet("0 a b");
    CHECK(vecEq(parsed, {0, 10, 11}), "parsePcSet a=10 b=11");
    CHECK(seqcalc::formatPcSet({0, 3, 7}) == "0 3 7", "formatPcSet round trip");
    CHECK(seqcalc::formatPcSet({0, 10, 11}) == "0 a b", "formatPcSet writes a and b");
}

static void testPatternEngine() {
    std::mt19937 rng(42);
    const std::vector<int> notes = {60, 64, 67};
    // "xxxx" at 4n: four 1-beat notes, velocity 100, notes ascend then wrap.
    auto rr = seqcalc::renderPattern(notes, "xxxx", std::nullopt, false, 1.0, 1, rng);
    CHECK(rr.events.size() == 4, "xxxx gives 4 events");
    CHECK(rr.events[0].midiNote == 60 && rr.events[0].startBeats == 0.0
              && rr.events[0].durationBeats == 1.0 && rr.events[0].velocity == 100,
          "first x: C4 at 0, 1 beat, vel 100");
    CHECK(rr.events[3].midiNote == 60, "notes wrap after the last note");
    CHECK(rr.lengthBeats == 4.0, "xxxx at 4n is 4 beats long");
    // Rest and sustain: "x_--" is one note sustained 2 beats, then rests.
    rr = seqcalc::renderPattern(notes, "x_--", std::nullopt, false, 1.0, 1, rng);
    CHECK(rr.events.size() == 1 && rr.events[0].durationBeats == 2.0,
          "x_--: one note sustained 2 beats, then rests");
    CHECK(rr.lengthBeats == 4.0, "x_-- is 4 beats long");
    // Brackets subdivide one step.
    rr = seqcalc::renderPattern(notes, "[xx]x", std::nullopt, false, 1.0, 1, rng);
    CHECK(rr.events.size() == 3, "[xx]x gives 3 events");
    CHECK(rr.events[0].durationBeats == 0.5 && rr.events[1].startBeats == 0.5,
          "bracket halves the step: 0.5-beat notes");
    CHECK(rr.lengthBeats == 2.0, "[xx]x is 2 beats long");
    // Repeat.
    rr = seqcalc::renderPattern(notes, "xx", std::nullopt, false, 1.0, 3, rng);
    CHECK(rr.events.size() == 6 && rr.lengthBeats == 6.0, "repeat 3 triples xx");
    // Subdiv 8n: half-beat steps.
    rr = seqcalc::renderPattern(notes, "xx", std::nullopt, false, 0.5, 1, rng);
    CHECK(rr.lengthBeats == 1.0 && rr.events[0].durationBeats == 0.5,
          "8n steps are 0.5 beats");
    // R with a random-note set: always plays, velocity 100 (draft §1.4).
    rr = seqcalc::renderPattern(notes, "R", notes, false, 1.0, 1, rng);
    CHECK(rr.events.size() == 1 && rr.events[0].velocity == 100,
          "R with a set: one event at vel 100");
    // R with restsForR: a coin flip, so run it 20 times and expect
    // both outcomes across the runs (seeded, deterministic).
    int rests = 0, notesOut = 0;
    for (int i = 0; i < 20; ++i) {
        auto r2 = seqcalc::renderPattern(notes, "R", std::nullopt, false, 1.0, 1, rng);
        if (r2.events.empty()) ++rests;
        else ++notesOut;
    }
    CHECK(rests > 0 && notesOut > 0, "R as coin flip gives both rests and notes");
    // Unknown chars are ignored.
    rr = seqcalc::renderPattern(notes, "x?x", std::nullopt, false, 1.0, 1, rng);
    CHECK(rr.events.size() == 2 && rr.lengthBeats == 2.0,
          "unknown chars are ignored but still take a step");
    // Subdiv names: measures are 4 beats each in 4/4 (Scribbletune).
    CHECK(seqcalc::subdivToBeats("4n") == 1.0, "4n = 1 beat");
    CHECK(seqcalc::subdivToBeats("8n") == 0.5, "8n = 0.5 beats");
    CHECK(seqcalc::subdivToBeats("16n") == 0.25, "16n = 0.25 beats");
    CHECK(seqcalc::subdivToBeats("1n") == 4.0, "1n = 4 beats");
    CHECK(seqcalc::subdivToBeats("1m") == 4.0, "1m = 4 beats");
    CHECK(seqcalc::subdivToBeats("2m") == 8.0, "2m = 8 beats");
    CHECK(seqcalc::subdivToBeats("bogus") == 1.0, "unknown subdiv falls back to 4n");
}

static void testClipRenderer() {
    seqcalc::CalcInputs in;
    in.rootPc = 0;
    in.baseOctave = 4;
    in.octaveRange = 1;
    in.pcSet = {0, 3, 7};
    in.pattern = "xxxx";
    in.subdiv = "4n";
    in.repeat = 1;
    in.randomSeed = 7;
    auto out = seqcalc::renderClip(in);
    CHECK(out.valid, "renderClip succeeds on the default input");
    CHECK(out.displayText == "C4 D#4 G4", "display text is C4 D#4 G4, got '%s'",
          out.displayText.c_str());
    CHECK(out.events.size() == 4, "renderClip gives 4 events");

    // Forte path: 6-31 fills the set with its prime form.
    in.forteNumber = "6-31";
    out = seqcalc::renderClip(in);
    CHECK(out.valid && out.forteUsed == "6-31", "Forte 6-31 applies");
    CHECK(vecEq(out.fortePrimeForm, {0, 1, 3, 5, 8, 9}), "6-31 prime form");
    CHECK(out.displayText == "C4 C#4 D#4 F4 G#4 A4", "6-31 display, got '%s'",
          out.displayText.c_str());
    // Unknown Forte number: invalid, error set.
    in.forteNumber = "9-99";
    out = seqcalc::renderClip(in);
    CHECK(!out.valid && !out.error.empty(), "unknown Forte number fails with an error");
    in.forteNumber.clear();
    // Root only.
    in.rootOnly = true;
    out = seqcalc::renderClip(in);
    CHECK(out.valid && out.noteList.size() == 1 && out.noteList[0] == 60,
          "rootOnly gives just C4");
    in.rootOnly = false;
    // Scale mode: major from C3.
    in.usePcSet = false;
    in.scaleRootPc = 0;
    in.scaleOctave = 3;
    in.scaleIndex = 0;
    out = seqcalc::renderClip(in);
    CHECK(out.valid && out.displayText == "C3 D3 E3 F3 G3 A3 B3",
          "scale mode major from C3, got '%s'", out.displayText.c_str());
}

static void testProcessor() {
    SeqCalcProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    // Default state: a calculated pattern exists.
    CHECK(proc.patternLengthBeats() == 4.0, "default pattern is 4 beats");
    CHECK(proc.noteDisplay() == "C4 D#4 G4", "default display C4 D#4 G4");
    CHECK(!proc.isStale(), "freshly constructed processor is not stale");

    // PC-set text entry.
    proc.setPcSetText("0 4 7");
    CHECK(proc.isStale(), "editing the PC set marks stale");
    CHECK(proc.calc(), "calc succeeds on 0 4 7");
    CHECK(proc.noteDisplay() == "C4 E4 G4", "C major triad display");
    CHECK(!proc.isStale(), "calc clears stale");

    // Forte entry: the prime form fills the PC set, then clears.
    proc.setForteText("6-Z12");
    CHECK(proc.calc(), "calc succeeds on Forte 6-Z12");
    CHECK(proc.pcSetText() == "0 1 2 4 6 7", "6-Z12 prime form fills the set, got '%s'",
          proc.pcSetText().c_str());
    CHECK(proc.forteText().empty(), "Forte field clears after a successful lookup");

    // Bad Forte number: calc fails, old pattern kept.
    proc.setForteText("9-99");
    CHECK(!proc.calc(), "calc fails on unknown Forte number");
    CHECK(!proc.calcError().empty(), "calc error is set");
    CHECK(proc.noteDisplay().find("C4") == 0, "old pattern kept after a failed calc");
    proc.setForteText("");

    // Grid: PC action toggles a pitch class. The 6-Z12 set already
    // contains pc 2, so pressing it removes it.
    proc.setGridAction(3);
    proc.gridButtonPressed(2);
    CHECK(proc.pcSetText() == "0 1 4 6 7",
          "grid PC removes pitch class 2, got '%s'", proc.pcSetText().c_str());
    proc.gridButtonPressed(2);
    CHECK(proc.pcSetText() == "0 1 4 6 7 2",
          "grid PC adds pitch class 2 back at the end, got '%s'",
          proc.pcSetText().c_str());
    // Grid: Root action.
    proc.setGridAction(0);
    proc.gridButtonPressed(5);
    CHECK(proc.calc(), "calc after grid root change");
    CHECK(proc.noteDisplay().find("F4") == 0, "root F: first note F4, got '%s'",
          proc.noteDisplay().c_str());
    // Grid: Rhy action appends a rhythm char.
    proc.setGridAction(4);
    proc.setPatternText("x");
    proc.gridButtonPressed(0);
    CHECK(proc.patternText() == "xx", "grid Rhy appends x");
    proc.gridButtonPressed(2);
    CHECK(proc.patternText() == "xx-", "grid Rhy appends -");
    // Grid: Forte action appends a Forte char.
    proc.setGridAction(5);
    proc.setForteText("");
    proc.gridButtonPressed(6);
    CHECK(proc.forteText() == "6", "grid Forte appends 6");
    proc.gridButtonPressed(10);
    CHECK(proc.forteText() == "6Z", "grid Forte appends Z");

    // clrS and RESET.
    proc.clearSet();
    CHECK(proc.pcSetText().empty() && proc.forteText().empty(), "clrS clears both fields");
    proc.resetToDefaults();
    CHECK(proc.pcSetText() == "0 3 7" && proc.patternText() == "xxxx",
          "RESET restores the default clip string");
    CHECK(proc.noteDisplay() == "C4 D#4 G4", "RESET display is C4 D#4 G4");

    // Calc -> Send: standalone playback emits the pattern as MIDI out.
    proc.calc();
    proc.setPlaying(true);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    int totalNoteOns = 0;
    int totalNoteOffs = 0;
    int firstNote = -1;
    for (int block = 0; block < 40; ++block) {
        buffer.clear();
        midi.clear();
        proc.processBlock(buffer, midi);
        for (const auto meta : midi) {
            if (meta.getMessage().isNoteOn()) {
                ++totalNoteOns;
                if (firstNote < 0) firstNote = meta.getMessage().getNoteNumber();
            }
            if (meta.getMessage().isNoteOff()) ++totalNoteOffs;
        }
    }
    // 4 beats at 120 bpm, 512-sample blocks: 40 blocks = ~0.46 s =
    // ~0.93 beats, so only the first note has fired so far.
    CHECK(totalNoteOns == 1, "first block run emits exactly the first note, got %d",
          totalNoteOns);
    CHECK(firstNote == 60, "first emitted note is C4 (60), got %d", firstNote);
    CHECK(totalNoteOffs == 0, "no note-offs yet in the first 0.93 beats");
    // Run longer: 240 blocks total = ~5.57 beats, so onsets at
    // 0,1,2,3,4,5 fire (6 note-ons) and ends at 1,2,3,4,5 fire
    // (5 note-offs). This also crosses the loop boundary at beat 4,
    // where the old loop's last note and the new loop's first note
    // are the same pitch.
    for (int block = 0; block < 200; ++block) {
        buffer.clear();
        midi.clear();
        proc.processBlock(buffer, midi);
        for (const auto meta : midi) {
            if (meta.getMessage().isNoteOn()) ++totalNoteOns;
            if (meta.getMessage().isNoteOff()) ++totalNoteOffs;
        }
    }
    CHECK(totalNoteOns == 6, "playback loops: 6 note-ons over ~5.57 beats, got %d",
          totalNoteOns);
    CHECK(totalNoteOffs == 5, "playback loops: 5 note-offs over ~5.57 beats, got %d",
          totalNoteOffs);
    // Stop silences everything: no note-ons after STOP, and a
    // note-off for the sounding note.
    proc.setPlaying(false);
    buffer.clear();
    midi.clear();
    proc.processBlock(buffer, midi);
    int noteOnsAfterStop = 0, noteOffsAfterStop = 0;
    for (const auto meta : midi) {
        if (meta.getMessage().isNoteOn()) ++noteOnsAfterStop;
        if (meta.getMessage().isNoteOff()) ++noteOffsAfterStop;
    }
    CHECK(noteOnsAfterStop == 0, "no note-ons after STOP");
    CHECK(noteOffsAfterStop >= 1, "STOP sends a note-off for the sounding note");

    // Drag export: a MIDI file with the rendered events.
    proc.setPlaying(false);
    auto file = proc.exportMidiFile();
    CHECK(file.existsAsFile(), "export writes a MIDI file");
    juce::MidiFile mf;
    {
        juce::FileInputStream in(file);
        CHECK(mf.readFrom(in), "exported file parses as a MIDI file");
    }
    CHECK(mf.getNumTracks() == 1, "exported file has one track");
    const auto* track = mf.getTrack(0);
    int exportedNoteOns = 0;
    for (int i = 0; i < track->getNumEvents(); ++i)
        if (track->getEventPointer(i)->message.isNoteOn()) ++exportedNoteOns;
    CHECK(exportedNoteOns == 4, "exported file has 4 note-ons, got %d", exportedNoteOns);
    file.deleteFile();
}

int main() {
    std::printf("SeqCalc v0.1.0 smoke test\n");
    testForteTable();
    testSetTheory();
    testPatternEngine();
    testClipRenderer();
    testProcessor();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
