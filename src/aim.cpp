#include "aim.hpp"

#include <limits>
#include <vector>

#include "offsets.hpp"
#include "target.hpp"

namespace rr::feature {
namespace {

struct Candidate {
    std::uint64_t address = 0;
    rr::math::Vec2 screen;
    float distance = 0.0f;
};

bool project(const rr::game::Snapshot& snap, const rr::math::Vec3& world, float screen_w,
             float screen_h, rr::math::Vec2& out) {
    return rr::math::world_to_screen(snap.view, rr::off::kMatrixRowMajor, world, screen_w,
                                     screen_h, out);
}

// Screen point for the requested hitbox. NearestBone is resolved here because it
// needs the projected bones; every other mode comes straight from the pure picker.
bool hitbox_screen_point(const rr::game::Snapshot& snap, const rr::game::Entity& ent,
                         rr::aimlogic::Hitbox box, float player_height, float screen_w,
                         float screen_h, const rr::math::Vec2& crosshair,
                         rr::math::Vec2& out) {
    if (box == rr::aimlogic::Hitbox::NearestBone) {
        std::vector<rr::math::Vec2> projected;
        projected.reserve(ent.bones.size());
        for (const auto& bone : ent.bones) {
            rr::math::Vec2 screen{std::numeric_limits<float>::quiet_NaN(),
                                  std::numeric_limits<float>::quiet_NaN()};
            if (rr::math::plausible_world(bone)) {
                rr::math::Vec2 candidate;
                if (project(snap, bone, screen_w, screen_h, candidate)) screen = candidate;
            }
            projected.push_back(screen);
        }
        const auto index = rr::aimlogic::nearest_point_index(projected, crosshair);
        if (!index.has_value()) return false;
        out = projected[*index];
        return true;
    }

    rr::math::Vec3 world;
    if (!rr::aimlogic::hitbox_world_point(ent, box, player_height, world)) return false;
    return project(snap, world, screen_w, screen_h, out);
}

} // namespace

void Aim::release() {
    rr::input::release_left();
    state_.firing = false;
}

void Aim::update(rr::ui::Overlay& overlay, const rr::game::Snapshot& snap,
                 const rr::cfg::Settings& settings, const rr::input::Input& input,
                 bool target_focused, bool menu_open) {
    state_.has_target = false;
    state_.target_address = 0;
    state_.target_distance_px = 0.0f;

    if (!target_focused || menu_open || !snap.has_view) {
        state_.locked_address = 0;
        release();
        return;
    }
    if (!settings.aim_enabled && !settings.trigger_enabled) {
        state_.locked_address = 0;
        release();
        return;
    }

    const float screen_w = static_cast<float>(overlay.width());
    const float screen_h = static_cast<float>(overlay.height());
    const rr::math::Vec2 crosshair = overlay.centre();
    const bool key_held = input.down(settings.aim_key);
    const float fov = static_cast<float>(settings.aim_fov_px);

    if (settings.aim_draw_fov) {
        overlay.circle(crosshair.x, crosshair.y, fov, rr::ui::kGrey, 1);
    }

    rr::aimlogic::Filter filter;
    filter.team_check = settings.aim_team_check;
    filter.visible_only = settings.aim_visible_only;
    filter.max_world_distance = settings.aim_max_world_distance;

    const rr::aimlogic::Hitbox box =
        rr::aimlogic::resolve_hitbox(rr::aimlogic::hitbox_from_int(settings.aim_hitbox));

    std::vector<Candidate> candidates;
    candidates.reserve(snap.entities.size());
    for (const auto& ent : snap.entities) {
        if (!rr::aimlogic::eligible(ent, snap.local, snap.has_local, filter)) continue;
        rr::math::Vec2 screen;
        if (!hitbox_screen_point(snap, ent, box, settings.player_height, screen_w, screen_h,
                                 crosshair, screen)) {
            continue;
        }
        Candidate candidate;
        candidate.address = ent.address;
        candidate.screen = screen;
        candidate.distance = rr::math::screen_distance(screen, crosshair);
        candidates.push_back(candidate);
    }

    if (!key_held) state_.locked_address = 0;

    const Candidate* chosen = nullptr;
    if (settings.aim_sticky && key_held && state_.locked_address != 0) {
        for (const auto& candidate : candidates) {
            if (candidate.address == state_.locked_address && candidate.distance <= fov) {
                chosen = &candidate;
                break;
            }
        }
    }
    if (chosen == nullptr) {
        for (const auto& candidate : candidates) {
            if (chosen == nullptr || candidate.distance < chosen->distance) {
                chosen = &candidate;
            }
        }
    }

    if (chosen == nullptr) {
        state_.locked_address = 0;
        state_.trigger_ready_tick = 0;
        release();
        return;
    }

    state_.has_target = true;
    state_.target_screen = chosen->screen;
    state_.target_address = chosen->address;
    state_.target_distance_px = chosen->distance;

    if (settings.show_target_line) {
        overlay.line(crosshair.x, crosshair.y, chosen->screen.x, chosen->screen.y,
                     rr::ui::kAccent, 1);
    }

    // Re-checked here, immediately before any input is emitted.
    const bool still_focused = target_focused && GetForegroundWindow() != nullptr;
    const bool in_aim_fov = chosen->distance <= fov;

    if (settings.aim_enabled && in_aim_fov && still_focused && key_held) {
        state_.locked_address = chosen->address;
        const rr::math::Vec2 step =
            rr::math::smooth_step(crosshair, chosen->screen, settings.aim_smoothing);
        const int dx = static_cast<int>(step.x * settings.aim_pixel_scale);
        const int dy = static_cast<int>(step.y * settings.aim_pixel_scale);
        rr::input::move_mouse(dx, dy);
    }

    if (settings.trigger_enabled && still_focused &&
        chosen->distance <= static_cast<float>(settings.trigger_fov_px)) {
        const std::uint64_t now = GetTickCount64();
        if (state_.trigger_ready_tick == 0) {
            state_.trigger_ready_tick =
                now + static_cast<std::uint64_t>(settings.trigger_delay_ms);
        }
        if (now >= state_.trigger_ready_tick) {
            rr::input::press_left();
            state_.firing = true;
        }
    } else {
        state_.trigger_ready_tick = 0;
        release();
    }
}

} // namespace rr::feature
