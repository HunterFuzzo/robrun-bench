#pragma once
// Target identity and build-dependent values. Every value here is either tagged
// with provenance or marked unset. Unset is never a real 0 -- the has_ flag says which.
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace rr::config {

// --- target identity -------------------------------------------------------
// [unset | build unknown | source: none | not verified]
// Pass --process on the command line until this is filled from a confirmed run.
inline constexpr std::string_view kProcessName = {};
inline constexpr bool kHasProcessName = false;

// [unset | build unknown | source: none | not verified]
// Main module SizeOfImage. Fills from the probe report, then pins the build.
inline constexpr std::size_t kExpectedImageSize = 0;
inline constexpr bool kHasExpectedImageSize = false;

// [unset | build unknown | source: none | not verified]
inline constexpr std::string_view kBuildTag = {};
inline constexpr bool kHasBuildTag = false;

// --- platform constants (not build-dependent) ------------------------------
inline constexpr std::uint64_t kUserSpaceMin = 0x10000ull;
inline constexpr std::uint64_t kUserSpaceMax = 0x7FFFFFFFFFFFull;
inline constexpr std::size_t kPageSize = 0x1000;
// Bounds the probe's own allocation. Refuse to slurp an image larger than this.
inline constexpr std::size_t kMaxImageBytes = 512ull * 1024ull * 1024ull;
// Bounds every hit list so a bad pattern cannot flood the report.
inline constexpr std::size_t kMaxHits = 64;

inline constexpr bool plausible_pointer(std::uint64_t value) noexcept {
    return value >= kUserSpaceMin && value <= kUserSpaceMax;
}

} // namespace rr::config
