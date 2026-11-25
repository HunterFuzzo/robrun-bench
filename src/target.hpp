#pragma once
// Target selection logic, kept pure so hitbox choice and filtering are checkable
// without the game or a window.
#include <cstddef>
#include <optional>
#include <vector>

#include "entity.hpp"
#include "math.hpp"
#include "offsets.hpp"

namespace rr::aimlogic {

enum class Hitbox : int {
    BodyPoint = 0,   // the position read, centred by the feet knob
    UpperPoint,      // derived from the uncalibrated player height
    HeadBone,        // needs kHeadBoneIndex
    ChestBone,       // needs kChestBoneIndex
    NearestBone,     // resolved in screen space by the caller
    Count
};

inline const char* hitbox_name(Hitbox box) {
    switch (box) {
        case Hitbox::BodyPoint:   return "body";
        case Hitbox::UpperPoint:  return "upper";
        case Hitbox::HeadBone:    return "head bone";
        case Hitbox::ChestBone:   return "chest bone";
        case Hitbox::NearestBone: return "nearest bone";
        case Hitbox::Count:       break;
    }
    return "body";
}

inline Hitbox hitbox_from_int(int value) {
    if (value < 0 || value >= static_cast<int>(Hitbox::Count)) return Hitbox::BodyPoint;
    return static_cast<Hitbox>(value);
}

// True when the requested hitbox has the offsets it needs on this build.
inline bool hitbox_available(Hitbox box) {
    switch (box) {
        case Hitbox::BodyPoint:   return true;
        case Hitbox::UpperPoint:  return true;
        case Hitbox::HeadBone:    return rr::off::kHasHeadBone;
        case Hitbox::ChestBone:   return rr::off::kHasChestBone;
        case Hitbox::NearestBone: return rr::off::kHasBones;
        case Hitbox::Count:       break;
    }
    return false;
}

// Falls back down the chain until something this build can actually read.
inline Hitbox resolve_hitbox(Hitbox wanted) {
    if (hitbox_available(wanted)) return wanted;
    if (wanted == Hitbox::NearestBone || wanted == Hitbox::HeadBone ||
        wanted == Hitbox::ChestBone) {
        return Hitbox::UpperPoint;
    }
    return Hitbox::BodyPoint;
}

struct Filter {
    bool team_check = true;
    bool visible_only = false;
    float max_world_distance = 0.0f;  // 0 disables the cull
};

inline bool is_teammate(const rr::game::Entity& ent, const rr::game::Entity& local,
                        bool has_local) {
    if (!has_local || !local.has_team || !ent.has_team) return false;
    return local.team == ent.team;
}

inline bool eligible(const rr::game::Entity& ent, const rr::game::Entity& local,
                     bool has_local, const Filter& filter) {
    if (!ent.has_position) return false;
    if (ent.has_health && ent.health <= 0) return false;
    if (filter.team_check && is_teammate(ent, local, has_local)) return false;
    if (filter.visible_only && ent.has_visible && !ent.visible) return false;
    if (filter.max_world_distance > 0.0f && ent.has_distance &&
        ent.distance_world > filter.max_world_distance) {
        return false;
    }
    return true;
}

inline bool bone_point(const rr::game::Entity& ent, int index, rr::math::Vec3& out) {
    if (index < 0) return false;
    const std::size_t i = static_cast<std::size_t>(index);
    if (i >= ent.bones.size()) return false;
    if (!rr::math::plausible_world(ent.bones[i])) return false;
    out = ent.bones[i];
    return true;
}

// World point for every mode except NearestBone, which needs screen space.
inline bool hitbox_world_point(const rr::game::Entity& ent, Hitbox box,
                               float player_height, rr::math::Vec3& out) {
    if (!ent.has_position) return false;
    const rr::math::Vec3 feet =
        rr::off::kPositionIsFeet ? ent.position
                                 : rr::game::raise(ent.position, -player_height * 0.5f);
    switch (box) {
        case Hitbox::BodyPoint:
            out = rr::game::raise(feet, player_height * 0.5f);
            return true;
        case Hitbox::UpperPoint:
            if (ent.has_head) {
                out = ent.head;
                return true;
            }
            out = rr::game::raise(feet, player_height * 0.9f);
            return true;
        case Hitbox::HeadBone:
            return bone_point(ent, rr::off::kHeadBoneIndex, out);
        case Hitbox::ChestBone:
            return bone_point(ent, rr::off::kChestBoneIndex, out);
        case Hitbox::NearestBone:
        case Hitbox::Count:
            return false;
    }
    return false;
}

// Index of the point closest to a screen position. Empty input reports not found.
inline std::optional<std::size_t> nearest_point_index(
    const std::vector<rr::math::Vec2>& points, const rr::math::Vec2& to) {
    std::optional<std::size_t> best;
    float best_distance = 0.0f;
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (!rr::math::finite(points[i])) continue;
        const float distance = rr::math::screen_distance(points[i], to);
        if (!best.has_value() || distance < best_distance) {
            best = i;
            best_distance = distance;
        }
    }
    return best;
}

} // namespace rr::aimlogic
