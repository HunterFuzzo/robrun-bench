#pragma once
// Per-frame snapshot types. Pure: no reads happen here, the world reader fills them.
#include <cstdint>
#include <string>
#include <vector>

#include "math.hpp"
#include "offsets.hpp"

namespace rr::game {

struct Entity {
    std::uint64_t address = 0;
    rr::math::Vec3 position;       // valid only when has_position
    rr::math::Vec3 head;           // valid only when has_head
    std::int32_t health = 0;
    std::int32_t team = 0;
    std::string name;
    bool has_position = false;
    bool has_head = false;
    bool has_health = false;
    bool has_team = false;
    bool has_name = false;
    bool visible = false;          // meaningful only when has_visible
    bool has_visible = false;
    std::vector<rr::math::Vec3> bones;   // world points, empty when bones are off
    float distance_world = 0.0f;   // from the local player, valid when has_distance
    bool has_distance = false;
    float distance_px = 0.0f;      // from the crosshair, filled by the aim pass
};

// Moves a world point along the up axis named in offsets.hpp. Pure, so the axis
// knob is checkable without the game running.
inline rr::math::Vec3 raise(const rr::math::Vec3& base, float amount) {
    rr::math::Vec3 out = base;
    if constexpr (rr::off::kUpAxisIndex == 0) {
        out.x += amount;
    } else if constexpr (rr::off::kUpAxisIndex == 1) {
        out.y += amount;
    } else {
        out.z += amount;
    }
    return out;
}

struct Snapshot {
    std::vector<Entity> entities;
    Entity local;
    bool has_local = false;
    rr::math::Mat4 view;
    bool has_view = false;
    std::uint64_t frame = 0;
    std::size_t rejected = 0;      // entries dropped as implausible this frame
};

} // namespace rr::game
