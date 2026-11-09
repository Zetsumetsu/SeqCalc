// SetTheory — the settheory.js translation, re-implemented in C++.
// Takes a pitch-class set and spells it out as note names across octaves.
// Order-preserving: the set is emitted in the order typed, repeated per
// octave. No sorting, no dedupe, no transforms (the original does none).
#pragma once

#include <string>
#include <vector>

namespace seqcalc {

// Parse a typed PC-set string into pitch classes. Accepts digits 0-9,
// 'a'/'A' = 10, 'b'/'B' = 11; anything else (spaces, commas, '=') is
// ignored, matching settheory.js's /[0-9a-b]/ filter.
std::vector<int> parsePcSet(const std::string& text);

// Format pitch classes back as a spaced string ("0 1 3 5 8 9").
std::string formatPcSet(const std::vector<int>& pcs);

// Translate a PC set to MIDI note numbers. For each octave o in
// 0..numOctaves-1, for each pitch class in order: pitch =
// (pc + rootPc) mod 12, midi = (baseOctave + o + 1) * 12 + pitch.
// (MIDI convention: C4 = 60, so octave 4 starts at 60.)
std::vector<int> pcSetToMidi(int rootPc, int baseOctave, int numOctaves,
                             const std::vector<int>& pcs);

// MIDI note number to name ("C4", "C#4"). Sharps, as in settheory.js.
std::string midiToName(int midi);

// Join MIDI notes as a space-separated name string ("C4 D4 F4").
std::string midiNames(const std::vector<int>& midiNotes);

} // namespace seqcalc
