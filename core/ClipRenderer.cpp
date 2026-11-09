// ClipRenderer implementation. See ClipRenderer.h.
#include "ClipRenderer.h"

#include <algorithm>

#include "ForteTable.h"
#include "ScaleLibrary.h"
#include "SetTheory.h"

namespace seqcalc {

CalcOutput renderClip(const CalcInputs& in) {
    CalcOutput out;
    const ForteTable& table = ForteTable::instance();

    // Step 1: Forte lookup replaces the PC set when a number is given.
    std::vector<int> pcSet = in.pcSet;
    if (!in.forteNumber.empty()) {
        auto row = table.lookup(in.forteNumber);
        if (!row.has_value()) {
            out.error = "Forte number not found: " + in.forteNumber;
            return out;
        }
        out.forteUsed = in.forteNumber;
        out.fortePrimeForm = row->primeForm;
        pcSet = row->primeForm;
    }

    // Steps 2-3: the note list.
    std::vector<int> scaleNotes;
    if (in.usePcSet) {
        scaleNotes = pcSetToMidi(in.rootPc, in.baseOctave, in.octaveRange, pcSet);
    } else {
        scaleNotes = scaleToMidi(in.scaleRootPc, in.scaleOctave, in.scaleIndex);
    }

    // Step 4: root-only reduces the list to the root note alone.
    std::vector<int> notes = scaleNotes;
    if (in.rootOnly) {
        const int oct = in.usePcSet ? in.baseOctave : in.scaleOctave;
        const int rpc = in.usePcSet ? in.rootPc : in.scaleRootPc;
        notes = {(oct + 1) * 12 + rpc};
    }

    // Steps 5-6: the random-note set.
    // restsForR -> no random-note set (R becomes a coin flip).
    std::optional<std::vector<int>> rNotes;
    if (!in.restsForR) {
        if (in.randomInScale) {
            rNotes = scaleNotes;
        } else {
            // Chromatic pitch classes NOT in the current set, as MIDI
            // notes in the working octave. (The original compares note
            // names in one octave; pitch classes are the same idea
            // without the octave bookkeeping.)
            std::vector<int> setPcs;
            if (in.usePcSet) {
                setPcs = pcSet;
            } else {
                const auto& scales = scaleLibrary();
                const int si = std::max(0, std::min((int)scales.size() - 1, in.scaleIndex));
                for (int iv : scales[(size_t)si].intervals)
                    setPcs.push_back((in.scaleRootPc + iv) % 12);
            }
            const int oct = in.usePcSet ? in.baseOctave : in.scaleOctave;
            std::vector<int> chromatic;
            for (int pc = 0; pc < 12; ++pc) {
                if (std::find(setPcs.begin(), setPcs.end(), pc) == setPcs.end())
                    chromatic.push_back((oct + 1) * 12 + pc);
            }
            rNotes = chromatic;
        }
    }

    out.noteList = notes;
    out.noteNames.clear();
    for (int m : notes) out.noteNames.push_back(midiToName(m));
    out.displayText = midiNames(notes);

    // Steps 7-8: render the pattern.
    std::mt19937 rng(in.randomSeed != 0
                         ? in.randomSeed
                         : std::random_device{}());
    RenderResult rr = renderPattern(notes, in.pattern, rNotes, in.shuffle,
                                    subdivToBeats(in.subdiv), in.repeat, rng);
    out.events = rr.events;
    out.lengthBeats = rr.lengthBeats;
    out.valid = true;
    return out;
}

} // namespace seqcalc
