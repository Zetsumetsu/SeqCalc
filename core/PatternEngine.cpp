// PatternEngine implementation. See PatternEngine.h.
#include "PatternEngine.h"

#include <algorithm>

namespace seqcalc {

bool isLegalPatternChar(char c) {
    return c == 'x' || c == 'R' || c == '-' || c == '_' || c == '[' || c == ']';
}

bool isValidPattern(const std::string& pattern) {
    for (char c : pattern)
        if (!isLegalPatternChar(c)) return false;
    return true;
}

std::string sanitizePattern(const std::string& pattern) {
    std::string out;
    for (char c : pattern)
        if (isLegalPatternChar(c)) out += c;
    return out;
}

double subdivToBeats(const std::string& subdiv) {
    if (subdiv == "1m") return 4.0;
    if (subdiv == "2m") return 8.0;
    if (subdiv == "3m") return 12.0;
    if (subdiv == "4m") return 16.0;
    if (subdiv == "1n") return 4.0;
    if (subdiv == "2n") return 2.0;
    if (subdiv == "4n") return 1.0;
    if (subdiv == "8n") return 0.5;
    if (subdiv == "16n") return 0.25;
    return 1.0; // unknown -> 4n
}

namespace {

// Count the top-level items in [begin, end): plain chars count 1 each,
// a bracket group counts 1 (its contents are subdivided recursively).
size_t countItems(const std::string& s, size_t begin, size_t end) {
    size_t count = 0;
    size_t i = begin;
    while (i < end) {
        if (s[i] == '[') {
            int depth = 1;
            ++i;
            while (i < end && depth > 0) {
                if (s[i] == '[') ++depth;
                else if (s[i] == ']') --depth;
                ++i;
            }
            ++count;
        } else if (s[i] == ']') {
            ++i; // stray close bracket: ignored, no time advance
        } else {
            ++count;
            ++i;
        }
    }
    return count;
}

struct Cursor {
    const std::vector<int>& notes;
    const std::optional<std::vector<int>>& randomNotes;
    std::mt19937& rng;
    size_t noteIndex = 0;
    std::vector<RenderedEvent>* events = nullptr;
    double now = 0.0;

    int nextNote() {
        if (notes.empty()) return -1;
        const int n = notes[noteIndex % notes.size()];
        ++noteIndex;
        return n;
    }
    void emit(int note, double dur, int velocity) {
        RenderedEvent e;
        e.startBeats = now;
        e.durationBeats = dur;
        e.midiNote = note;
        e.velocity = velocity;
        events->push_back(e);
    }
};

// Render the items in s[begin, end), each top-level item taking
// stepDur beats. Groups recurse with their step split evenly.
void renderSequence(const std::string& s, size_t begin, size_t end,
                    double stepDur, Cursor& cur) {
    size_t i = begin;
    while (i < end) {
        const char c = s[i];
        if (c == '[') {
            // Find the matching close bracket (nesting allowed). An
            // unmatched '[' groups to the end of the string.
            int depth = 1;
            size_t j = i + 1;
            while (j < end && depth > 0) {
                if (s[j] == '[') ++depth;
                else if (s[j] == ']') --depth;
                ++j;
            }
            const size_t innerEnd = (depth == 0) ? j - 1 : end;
            const size_t items = std::max<size_t>(1, countItems(s, i + 1, innerEnd));
            // The recursive call advances cur.now by exactly stepDur
            // (items x stepDur/items), so do not advance it again here.
            renderSequence(s, i + 1, innerEnd, stepDur / (double)items, cur);
            i = (depth == 0) ? j : end;
        } else if (c == ']') {
            ++i; // stray close bracket: ignored
        } else if (c == 'x') {
            const int n = cur.nextNote();
            if (n >= 0) cur.emit(n, stepDur, 100);
            cur.now += stepDur;
            ++i;
        } else if (c == 'R') {
            if (cur.randomNotes.has_value() && !cur.randomNotes->empty()) {
                std::uniform_int_distribution<size_t> pick(
                    0, cur.randomNotes->size() - 1);
                cur.emit((*cur.randomNotes)[pick(cur.rng)], stepDur, 100);
            } else {
                // No random-note set: a coin flip. Heads plays the next
                // note from the main list at velocity 70; tails is silent.
                std::uniform_int_distribution<int> coin(0, 1);
                if (coin(cur.rng) == 1) {
                    const int n = cur.nextNote();
                    if (n >= 0) cur.emit(n, stepDur, 70);
                }
            }
            cur.now += stepDur;
            ++i;
        } else if (c == '-') {
            cur.now += stepDur;
            ++i;
        } else if (c == '_') {
            // Sustain: extend the previous event by one step. With no
            // previous event there is nothing to extend; time advances.
            if (!cur.events->empty())
                cur.events->back().durationBeats += stepDur;
            cur.now += stepDur;
            ++i;
        } else {
            ++i; // illegal chars were sanitised out; skip defensively
        }
    }
}

} // namespace

RenderResult renderPattern(const std::vector<int>& notes,
                           const std::string& pattern,
                           const std::optional<std::vector<int>>& randomNotes,
                           bool shuffle, double stepBeats, int repeat,
                           std::mt19937& rng) {
    RenderResult out;
    std::string clean = sanitizePattern(pattern);
    if (clean.empty() || repeat < 1) return out;
    std::string full;
    for (int r = 0; r < repeat; ++r) full += clean;

    std::vector<int> noteList = notes;
    if (shuffle && noteList.size() > 1)
        std::shuffle(noteList.begin(), noteList.end(), rng);

    Cursor cur{noteList, randomNotes, rng, 0, &out.events, 0.0};
    renderSequence(full, 0, full.size(), stepBeats, cur);
    out.lengthBeats = cur.now;
    return out;
}

} // namespace seqcalc
