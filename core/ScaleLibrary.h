// ScaleLibrary — scale interval lists for scale mode (USE PC SET off).
// The list follows the sibling projects' scale menu: major, natural
// minor, dorian, mixolydian, pentatonic major, pentatonic minor,
// chromatic. Major is the confirmed one from the device screenshot;
// the rest are the same Tonal-style names the original menus used.
#pragma once

#include <string>
#include <vector>

namespace seqcalc {

struct ScaleDef {
    std::string name;
    std::vector<int> intervals; // semitone offsets from the root, one octave
};

inline const std::vector<ScaleDef>& scaleLibrary() {
    static const std::vector<ScaleDef> scales = {
        {"major",            {0, 2, 4, 5, 7, 9, 11}},
        {"natural minor",    {0, 2, 3, 5, 7, 8, 10}},
        {"dorian",           {0, 2, 3, 5, 7, 9, 10}},
        {"mixolydian",       {0, 2, 4, 5, 7, 9, 10}},
        {"pentatonic major", {0, 2, 4, 7, 9}},
        {"pentatonic minor", {0, 3, 5, 7, 10}},
        {"chromatic",        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    };
    return scales;
}

// Scale notes as MIDI numbers: root pitch class at the given octave,
// one octave of the scale (no octave repeat).
inline std::vector<int> scaleToMidi(int rootPc, int octave, int scaleIndex) {
    const auto& scales = scaleLibrary();
    if (scaleIndex < 0 || scaleIndex >= (int)scales.size()) scaleIndex = 0;
    std::vector<int> out;
    for (int iv : scales[(size_t)scaleIndex].intervals)
        out.push_back((octave + 1) * 12 + (rootPc + iv) % 12);
    return out;
}

} // namespace seqcalc
