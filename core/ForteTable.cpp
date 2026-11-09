// ForteTable implementation. See ForteTable.h.
#include "ForteTable.h"
#include "ForteTableData.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace seqcalc {

const ForteTable& ForteTable::instance() {
    static const ForteTable table;
    return table;
}

static ForteRowView toView(const ForteRow& r) {
    ForteRowView v;
    v.number = r.number;
    v.primeForm = ForteTable::parsePrimeForm(r.primeForm);
    v.intervalVector = r.intervalVector;
    return v;
}

std::optional<ForteRowView> ForteTable::lookup(const std::string& forteNumber) const {
    // Exact match only (no fuzzy matching in v0.1). Whitespace is
    // stripped and a lowercase z is normalised to Z, so " 6-z12 "
    // finds 6-Z12; the number itself must otherwise match exactly.
    std::string key;
    for (char c : forteNumber) {
        if (std::isspace((unsigned char)c)) continue;
        key += (c == 'z' ? 'Z' : c);
    }
    for (const auto& row : kForteTable)
        if (key == row.number) return toView(row);
    return std::nullopt;
}

std::optional<ForteRowView> ForteTable::rowAt(int index) const {
    if (index < 0 || index >= (int)kForteTable.size()) return std::nullopt;
    return toView(kForteTable[(size_t)index]);
}

int ForteTable::indexOf(const std::string& forteNumber) const {
    std::string key;
    for (char c : forteNumber) {
        if (std::isspace((unsigned char)c)) continue;
        key += (c == 'z' ? 'Z' : c);
    }
    for (size_t i = 0; i < kForteTable.size(); ++i)
        if (key == kForteTable[i].number) return (int)i;
    return -1;
}

std::string ForteTable::intervalVectorFor(const std::vector<int>& primeForm) {
    int v[6] = {0, 0, 0, 0, 0, 0};
    std::vector<int> pcs = primeForm;
    std::sort(pcs.begin(), pcs.end());
    pcs.erase(std::unique(pcs.begin(), pcs.end()), pcs.end());
    for (size_t i = 0; i < pcs.size(); ++i) {
        for (size_t j = i + 1; j < pcs.size(); ++j) {
            int d = (pcs[j] - pcs[i]) % 12;
            if (d < 0) d += 12;
            int ic = std::min(d, 12 - d);
            if (ic >= 1 && ic <= 6) v[ic - 1]++;
        }
    }
    std::string out;
    for (int k = 0; k < 6; ++k) out += (char)('0' + v[k]);
    return out;
}

std::vector<int> ForteTable::parsePrimeForm(const std::string& text) {
    std::vector<int> out;
    std::string cur;
    auto flush = [&]() {
        if (cur.empty()) return true;
        if (cur == "T" || cur == "t") { out.push_back(10); }
        else if (cur == "E" || cur == "e") { out.push_back(11); }
        else {
            try {
                size_t used = 0;
                int v = std::stoi(cur, &used);
                if (used != cur.size()) return false;
                out.push_back(v);
            } catch (...) { return false; }
        }
        cur.clear();
        return true;
    };
    for (char c : text) {
        if (c == ',' || c == ' ') { if (!flush()) return {}; }
        else cur += c;
    }
    if (!flush()) return {};
    return out;
}

std::vector<std::string> ForteTable::validate() const {
    std::vector<std::string> errors;
    if ((int)kForteTable.size() != kRowCount)
        errors.push_back("row count is not 206");
    std::set<std::string> numbers;
    std::set<std::vector<int>> primeForms;
    std::map<std::string, std::vector<std::string>> zByIv;
    for (const auto& row : kForteTable) {
        const std::string fn = row.number;
        if (!numbers.insert(fn).second)
            errors.push_back(fn + ": duplicate Forte number");
        std::vector<int> pf = parsePrimeForm(row.primeForm);
        const int n = (int)std::stoi(fn.substr(0, fn.find('-')));
        if ((int)pf.size() != n)
            errors.push_back(fn + ": prime form length " +
                             std::to_string(pf.size()) + " != " + std::to_string(n));
        bool ascending = !pf.empty() && pf[0] == 0;
        for (size_t i = 1; i < pf.size(); ++i)
            if (pf[i] <= pf[i - 1]) ascending = false;
        if (!ascending)
            errors.push_back(fn + ": prime form is not ascending from 0: " +
                             std::string(row.primeForm));
        for (int p : pf)
            if (p < 0 || p > 11)
                errors.push_back(fn + ": prime form value out of range: " +
                                 std::string(row.primeForm));
        if (!primeForms.insert(pf).second)
            errors.push_back(fn + ": duplicate prime form " + std::string(row.primeForm));
        const std::string iv = row.intervalVector;
        if (iv.size() != 6)
            errors.push_back(fn + ": interval vector length is not 6: " + iv);
        int sum = 0;
        for (char c : iv) {
            if (c < '0' || c > '9') { errors.push_back(fn + ": interval vector not digits: " + iv); break; }
            sum += c - '0';
        }
        if (sum != n * (n - 1) / 2)
            errors.push_back(fn + ": interval vector digit sum " + std::to_string(sum) +
                             " != " + std::to_string(n * (n - 1) / 2));
        if (intervalVectorFor(pf) != iv)
            errors.push_back(fn + ": recomputed interval vector " + intervalVectorFor(pf) +
                             " != stored " + iv);
        if (fn.find('Z') != std::string::npos) zByIv[iv].push_back(fn);
    }
    for (const auto& [iv, fns] : zByIv)
        if (fns.size() != 2)
            errors.push_back("Z interval vector " + iv + " has " +
                             std::to_string(fns.size()) + " rows, expected a pair");
    return errors;
}

} // namespace seqcalc
