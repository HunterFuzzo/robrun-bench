#pragma once
// Every build-dependent value for the feature set lives here and nowhere else.
// All of them are unset. Zero is the unset marker; each kHas* gate derives itself
// from its value, so filling a real number in is a one-line edit and the feature
// turns itself on. No invented address sits in this file -- a fake 0x1A2B3C0 reads
// exactly like a confirmed one three weeks from now.
#include <array>
#include <cstddef>
#include <cstdint>

namespace rr::off {

// --- module-relative, main module base + value ------------------------------
// [unset | build unknown | source: none | not verified]
inline constexpr std::uint64_t kEntityList = 0;
// [unset | build unknown | source: none | not verified]
inline constexpr std::uint64_t kLocalPlayer = 0;
// [unset | build unknown | source: none | not verified]  float[16]
inline constexpr std::uint64_t kViewMatrix = 0;
// [unset | build unknown | source: none | not verified]  int32 live entity count
inline constexpr std::uint64_t kEntityCount = 0;

// --- entity-relative --------------------------------------------------------
// [unset | build unknown | source: none | not verified]  float[3]
inline constexpr std::uint32_t kEntPosition = 0;
// [unset | build unknown | source: none | not verified]  float[3], second aim point
inline constexpr std::uint32_t kEntHeadPosition = 0;
// [unset | build unknown | source: none | not verified]  int32
inline constexpr std::uint32_t kEntHealth = 0;
// [unset | build unknown | source: none | not verified]  int32
inline constexpr std::uint32_t kEntTeam = 0;
// [unset | build unknown | source: none | not verified]  char[kNameLength]
inline constexpr std::uint32_t kEntName = 0;
// [unset | build unknown | source: none | not verified]  int32, 0 = not in world
inline constexpr std::uint32_t kEntAlive = 0;
// [unset | build unknown | source: none | not verified]  int32, 0 = not visible
inline constexpr std::uint32_t kEntVisible = 0;
// [unset | build unknown | source: none | not verified]  bone array, see the knobs below
inline constexpr std::uint32_t kEntBoneArray = 0;

// --- layout knobs, unconfirmed until a dump says otherwise ------------------
// 0 keeps the pointer-array reading. A non-zero stride switches to an inline
// array of structs at kEntityList with this byte stride.
inline constexpr std::size_t kEntityStride = 0;
inline constexpr std::size_t kEntityCountFallback = 64;   // used when kEntityCount is unset
inline constexpr std::size_t kEntityCountMax = 1024;      // hard bound on the loop
inline constexpr std::size_t kNameLength = 32;

// Flip this if the overlay mirrors or stands on its head. Both paths are written.
inline constexpr bool kMatrixRowMajor = true;
// Position read is the feet point; the box grows upward from it. Flip if it is the centre.
inline constexpr bool kPositionIsFeet = true;
// Which component of the position triple points up. 0 = x, 1 = y, 2 = z.
// Unconfirmed: if the boxes lie on their side, this is the line to change.
inline constexpr std::size_t kUpAxisIndex = 2;

// --- bone array knobs -------------------------------------------------------
// The array is either a pointer stored at entity + kEntBoneArray, or the entries
// themselves start there. Each entry is kBoneStride bytes and carries a float[3]
// position at kBonePositionOffset inside it -- for a 3x4 transform that offset is
// the translation column, not the start of the entry.
inline constexpr bool kBoneArrayIsPointer = true;
inline constexpr std::size_t kBoneStride = 0;          // 0 keeps bones switched off
inline constexpr std::size_t kBonePositionOffset = 0;
inline constexpr std::size_t kBoneCount = 0;           // entries to walk
inline constexpr std::size_t kBoneCountMax = 256;      // hard bound on the loop

// -1 is unset here, because bone 0 is a real index.
inline constexpr int kHeadBoneIndex = -1;
inline constexpr int kChestBoneIndex = -1;

struct BonePair {
    int parent;
    int child;
};

// Fill with the pairs that make a skeleton on this build, for example
// {{0, 1}, {1, 2}, {2, 3}}. An empty chain leaves the skeleton off.
inline constexpr std::array<BonePair, 0> kBoneChain{};

// --- derived gates ----------------------------------------------------------
inline constexpr bool kHasEntityList = kEntityList != 0;
inline constexpr bool kHasLocalPlayer = kLocalPlayer != 0;
inline constexpr bool kHasViewMatrix = kViewMatrix != 0;
inline constexpr bool kHasEntityCount = kEntityCount != 0;
inline constexpr bool kHasPosition = kEntPosition != 0;
inline constexpr bool kHasHeadPosition = kEntHeadPosition != 0;
inline constexpr bool kHasHealth = kEntHealth != 0;
inline constexpr bool kHasTeam = kEntTeam != 0;
inline constexpr bool kHasName = kEntName != 0;
inline constexpr bool kHasAlive = kEntAlive != 0;
inline constexpr bool kHasVisible = kEntVisible != 0;
inline constexpr bool kHasBones = kEntBoneArray != 0 && kBoneStride != 0 && kBoneCount != 0;
inline constexpr bool kHasHeadBone = kHasBones && kHeadBoneIndex >= 0;
inline constexpr bool kHasChestBone = kHasBones && kChestBoneIndex >= 0;
inline constexpr bool kHasBoneChain = kHasBones && !kBoneChain.empty();
inline constexpr bool kEntityListIsPointerArray = kEntityStride == 0;

// What a feature needs before it is allowed to run.
inline constexpr bool kCanWorldToScreen = kHasViewMatrix;
inline constexpr bool kCanEnumerate = kHasEntityList && kHasPosition;
inline constexpr bool kCanEsp = kCanEnumerate && kCanWorldToScreen;
inline constexpr bool kCanTeamCheck = kHasTeam && kHasLocalPlayer;
inline constexpr bool kCanAim = kCanEsp;
inline constexpr bool kCanSkeleton = kCanWorldToScreen && kHasBoneChain;
inline constexpr bool kCanVisibilityFilter = kHasVisible;
inline constexpr bool kCanWorldDistance = kHasLocalPlayer && kHasPosition;

} // namespace rr::off
