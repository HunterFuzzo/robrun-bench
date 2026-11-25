#pragma once
// Pure byte-level search. No platform calls, so it is checkable without the game.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rr::scan {

struct PatternByte {
    std::uint8_t value = 0;
    bool wildcard = false;
};

// Parses IDA-style "48 8B 05 ?? ?? ?? ?? 48 85 C0". Accepts ? or ?? for a hole.
// Returns an empty vector on malformed input.
inline std::vector<PatternByte> parse_pattern(std::string_view text) {
    std::vector<PatternByte> out;
    std::size_t i = 0;
    const auto hex_value = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    while (i < text.size()) {
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
        if (i >= text.size()) break;
        if (text[i] == '?') {
            ++i;
            if (i < text.size() && text[i] == '?') ++i;
            out.push_back(PatternByte{0, true});
            continue;
        }
        const int hi = hex_value(text[i]);
        if (hi < 0) return {};
        ++i;
        if (i >= text.size()) return {};
        const int lo = hex_value(text[i]);
        if (lo < 0) return {};
        ++i;
        out.push_back(PatternByte{static_cast<std::uint8_t>((hi << 4) | lo), false});
    }
    return out;
}

// A copy of a module image plus a per-page readability mask. An absent page is
// zero-filled in bytes and never matches -- a hole is failure, not zero.
struct ImageView {
    const std::uint8_t* bytes = nullptr;
    std::size_t size = 0;
    const std::uint8_t* page_present = nullptr; // one byte per page; null means all present
    std::size_t page_size = 0x1000;
};

inline bool range_present(const ImageView& img, std::size_t offset, std::size_t length) {
    if (length == 0) return false;
    if (offset > img.size || length > img.size - offset) return false;
    if (img.page_present == nullptr) return true;
    const std::size_t first = offset / img.page_size;
    const std::size_t last = (offset + length - 1) / img.page_size;
    for (std::size_t p = first; p <= last; ++p) {
        if (img.page_present[p] == 0) return false;
    }
    return true;
}

inline bool match_at(const ImageView& img, std::size_t offset, const std::vector<PatternByte>& pat) {
    if (!range_present(img, offset, pat.size())) return false;
    for (std::size_t k = 0; k < pat.size(); ++k) {
        if (pat[k].wildcard) continue;
        if (img.bytes[offset + k] != pat[k].value) return false;
    }
    return true;
}

inline std::vector<std::size_t> find_pattern_all(const ImageView& img,
                                                 const std::vector<PatternByte>& pat,
                                                 std::size_t max_hits) {
    std::vector<std::size_t> hits;
    if (pat.empty() || img.bytes == nullptr || img.size < pat.size()) return hits;
    const std::size_t limit = img.size - pat.size();
    for (std::size_t off = 0; off <= limit; ++off) {
        if (match_at(img, off, pat)) {
            hits.push_back(off);
            if (hits.size() >= max_hits) break;
        }
    }
    return hits;
}

inline std::optional<std::size_t> find_pattern(const ImageView& img,
                                               const std::vector<PatternByte>& pat) {
    const auto hits = find_pattern_all(img, pat, 1);
    if (hits.empty()) return std::nullopt;
    return hits.front();
}

inline std::vector<std::size_t> find_ascii(const ImageView& img, std::string_view needle,
                                           std::size_t max_hits) {
    std::vector<PatternByte> pat;
    pat.reserve(needle.size());
    for (const char c : needle) {
        pat.push_back(PatternByte{static_cast<std::uint8_t>(c), false});
    }
    return find_pattern_all(img, pat, max_hits);
}

inline std::vector<std::size_t> find_utf16(const ImageView& img, std::string_view needle_ascii,
                                           std::size_t max_hits) {
    std::vector<PatternByte> pat;
    pat.reserve(needle_ascii.size() * 2);
    for (const char c : needle_ascii) {
        pat.push_back(PatternByte{static_cast<std::uint8_t>(c), false});
        pat.push_back(PatternByte{0, false});
    }
    return find_pattern_all(img, pat, max_hits);
}

// True when the marker is present as ASCII or as UTF-16LE anywhere in the image.
inline bool contains_marker(const ImageView& img, std::string_view marker) {
    if (!find_ascii(img, marker, 1).empty()) return true;
    return !find_utf16(img, marker, 1).empty();
}

} // namespace rr::scan
