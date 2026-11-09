// SetTheory implementation. See SetTheory.h.
#include "SetTheory.h"

namespace seqcalc {

std::vector<int> parsePcSet(const std::string& text) {
    std::vector<int> out;
    for (char c : text) {
        if (c >= '0' && c <= '9') out.push_back(c - '0');
        else if (c == 'a' || c == 'A') out.push_back(10);
        else if (c == 'b' || c == 'B') out.push_back(11);
        // anything else is ignored, as in settheory.js
    }
    return out;
}

std::string formatPcSet(const std::vector<int>& pcs) {
    std::string out;
    for (size_t i = 0; i < pcs.size(); ++i) {
        if (i) out += ' ';
        const int p = pcs[i];
        if (p == 10) out += 'a';
        else if (p == 11) out += 'b';
        else out += (char)('0' + p);
    }
    return out;
}

std::vector<int> pcSetToMidi(int rootPc, int baseOctave, int numOctaves,
                             const std::vector<int>& pcs) {
    std::vector<int> out;
    if (pcs.empty() || numOctaves < 1) return out;
    for (int o = 0; o < numOctaves; ++o) {
        for (int pc : pcs) {
            const int pitch = (pc + rootPc) % 12;
            out.push_back((baseOctave + o + 1) * 12 + pitch);
        }
    }
    return out;
}

std::string midiToName(int midi) {
    static const char* names[12] = {"C",  "C#", "D",  "D#", "E",  "F",
                                    "F#", "G",  "G#", "A",  "A#", "B"};
    const int pc = ((midi % 12) + 12) % 12;
    const int oct = midi / 12 - 1;
    return std::string(names[pc]) + std::to_string(oct);
}

std::string midiNames(const std::vector<int>& midiNotes) {
    std::string out;
    for (size_t i = 0; i < midiNotes.size(); ++i) {
        if (i) out += ' ';
        out += midiToName(midiNotes[i]);
    }
    return out;
}

} // namespace seqcalc
