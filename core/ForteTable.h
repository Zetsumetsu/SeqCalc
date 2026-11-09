// ForteTable — Allen Forte's pitch-class set table (The Structure of
// Atonal Music, 1973), as transcribed by Steve Zydek into forte.js
// (SeqCalc), with the six §3.1 fixes applied and Forte's prime forms on
// the six divergent rows. See VST-PORT-DRAFT.md §3.
// The table data lives in ForteTableData.h (generated, do not hand-edit).
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace seqcalc {

struct ForteRowView {
    std::string number;    // e.g. "6-31", "6-Z12"
    std::vector<int> primeForm;
    std::string intervalVector; // 6 digits
};

class ForteTable {
public:
    static constexpr int kRowCount = 206;

    static const ForteTable& instance();

    // Exact-match lookup by Forte number string ("6-31", "6-Z12").
    // Returns nullopt when the string is not a Forte number in the table.
    std::optional<ForteRowView> lookup(const std::string& forteNumber) const;

    // Row by index 0..205 (table order: cardinality, then number).
    std::optional<ForteRowView> rowAt(int index) const;
    int indexOf(const std::string& forteNumber) const; // -1 when absent

    // Recompute an interval vector from a prime form. Used by the
    // §3.3 validation and by the tests; it is the check the October
    // audit lacked, and it catches digit-permutation glitches.
    static std::string intervalVectorFor(const std::vector<int>& primeForm);

    // Parse a prime-form string ("0,1,3,5,8,9", "0,2,4,6,8,10").
    // Returns empty on unparseable input.
    static std::vector<int> parsePrimeForm(const std::string& text);

    // §3.3 validation rules, applied to all 206 rows. Returns the list
    // of failures; empty means the table is valid. Runs at load time
    // (processor constructor, debug builds assert on failure) and in
    // the unit tests.
    std::vector<std::string> validate() const;

private:
    ForteTable() = default;
};

} // namespace seqcalc
