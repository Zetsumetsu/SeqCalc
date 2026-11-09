// PatternEngine — the Scribbletune clip() pattern language,
// re-implemented in C++ from its documented semantics (VST-PORT-DRAFT
// §1.4). The vendored scribble1.js bundle is NOT embedded.
// Pattern characters:
//   x  play the next note from the note list, in order (index wraps)
//   R  random note (see below)
//   -  silent step, time advances, no note
//   _  sustain: extend the previous event by one step
//   [ ] subdivide: the bracketed group splits one step evenly
// Velocities: x = 100 (amp); R from the random-note set = 100;
// R falling back to the main list = 70 (accentLow).
#pragma once

#include <optional>
#include <random>
#include <string>
#include <vector>

namespace seqcalc {

struct RenderedEvent {
    double startBeats = 0.0;
    double durationBeats = 0.0;
    int midiNote = 60;
    int velocity = 100; // 0..127
};

struct RenderResult {
    std::vector<RenderedEvent> events;
    double lengthBeats = 0.0; // total pattern length in beats
};

// Only these characters are legal in a pattern.
bool isLegalPatternChar(char c);
// True when every character is legal (empty pattern is legal).
bool isValidPattern(const std::string& pattern);
// Remove illegal characters (no time advance for them in v0.1).
std::string sanitizePattern(const std::string& pattern);

// Step duration in beats for a Scribbletune subdivision string.
// 1m=4, 2m=8, 3m=12, 4m=16, 1n=4, 2n=2, 4n=1, 8n=0.5, 16n=0.25.
// Unknown strings fall back to 4n (1 beat).
double subdivToBeats(const std::string& subdiv);

// Render a pattern against a note list.
//   notes: the ordered note list (MIDI numbers); x cycles through it.
//   randomNotes: nullopt means no random-note set (R becomes a coin
//     flip: 50% next note at velocity 70, 50% silent). Otherwise R
//     always plays a random pick from this list at velocity 100.
//   shuffle: shuffle the note list before mapping (Fisher-Yates).
//   stepBeats: duration of one step, from subdivToBeats.
//   repeat: the pattern string is repeated this many times first.
// The render is deterministic for a given rng state.
RenderResult renderPattern(const std::vector<int>& notes,
                           const std::string& pattern,
                           const std::optional<std::vector<int>>& randomNotes,
                           bool shuffle, double stepBeats, int repeat,
                           std::mt19937& rng);

} // namespace seqcalc
