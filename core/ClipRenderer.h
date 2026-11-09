// ClipRenderer — the §2.6 calculation pipeline, assembled from
// ForteTable, SetTheory, ScaleLibrary and PatternEngine.
//   1. pcSet = forteNumber ? forteTable[forteNumber].primeForm
//                            : typedOrGridPcSet
//   2. noteNames = setTheory(root, baseOctave, numOctaves, pcSet)
//   3. scaleNotes = usePcSet ? noteNames : scaleNotes(scale)
//   4. notes = rootOnly ? [root] : scaleNotes
//   5. crNotes = chromatic scale from root
//   6. rNotes = useScaleNotesForR ? scaleNotes
//                                : (crNotes minus scaleNotes)
//   7. pattern = typedPattern repeated repeatPattern times
//   8. events = renderPattern(notes, pattern, rNotes, shuffle, subdiv)
// Fidelity notes (draft §2.6): settheory does not sort or dedupe, so
// the PC set is emitted in the order typed. The Forte lookup is an
// exact match on the Forte number string.
#pragma once

#include <optional>
#include <random>
#include <string>
#include <vector>

#include "PatternEngine.h"

namespace seqcalc {

struct CalcInputs {
    int rootPc = 0;            // 0..11, C..B
    int baseOctave = 4;        // 0..10 (params.js cap)
    int octaveRange = 1;       // 1..8 (params.js cap)
    bool usePcSet = true;
    std::vector<int> pcSet;    // pitch classes, order preserved
    // Forte lookup: when non-empty, the prime form replaces pcSet.
    // Exact match on the Forte number string ("6-31", "6-Z12").
    std::string forteNumber;
    int scaleRootPc = 0;
    int scaleOctave = 3;
    int scaleIndex = 0;        // into ScaleLibrary
    std::string pattern = "xxxx";
    std::string subdiv = "4n";
    int repeat = 1;
    bool shuffle = false;
    bool randomInScale = true;
    bool rootOnly = false;
    bool restsForR = false;
    unsigned int randomSeed = 0; // 0 = nondeterministic seed
};

struct CalcOutput {
    std::vector<int> noteList;       // MIDI notes the pattern draws from
    std::vector<std::string> noteNames;
    std::string displayText;         // "C4 D4 F4" (empty when no notes)
    std::vector<RenderedEvent> events;
    double lengthBeats = 0.0;
    bool valid = false;
    std::string error;               // set when !valid
    std::string forteUsed;           // Forte number actually applied, if any
    std::vector<int> fortePrimeForm; // its prime form, if any
};

// Run the full pipeline. Deterministic for a fixed randomSeed.
CalcOutput renderClip(const CalcInputs& in);

} // namespace seqcalc
