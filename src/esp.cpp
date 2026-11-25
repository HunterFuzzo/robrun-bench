#include "esp.hpp"

#include <cstdio>
#include <string>

#include "offsets.hpp"
#include "target.hpp"

namespace rr::feature {
namespace {

std::string format_number(float value, int decimals) {
    char buffer[40]{};
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, static_cast<double>(value));
    return buffer;
}

bool project(const rr::game::Snapshot& snap, const rr::math::Vec3& world, float screen_w,
             float screen_h, rr::math::Vec2& out) {
    return rr::math::world_to_screen(snap.view, rr::off::kMatrixRowMajor, world, screen_w,
                                     screen_h, out);
}

void draw_skeleton(rr::ui::Overlay& overlay, const rr::game::Snapshot& snap,
                   const rr::game::Entity& ent, float screen_w, float screen_h,
                   COLORREF colour) {
    for (const auto& pair : rr::off::kBoneChain) {
        rr::math::Vec3 parent_world;
        rr::math::Vec3 child_world;
        if (!rr::aimlogic::bone_point(ent, pair.parent, parent_world)) continue;
        if (!rr::aimlogic::bone_point(ent, pair.child, child_world)) continue;
        rr::math::Vec2 parent_screen;
        rr::math::Vec2 child_screen;
        if (!project(snap, parent_world, screen_w, screen_h, parent_screen)) continue;
        if (!project(snap, child_world, screen_w, screen_h, child_screen)) continue;
        overlay.line(parent_screen.x, parent_screen.y, child_screen.x, child_screen.y,
                     colour, 1);
    }
}

} // namespace

void draw_esp(rr::ui::Overlay& overlay, const rr::game::Snapshot& snap,
              const rr::cfg::Settings& settings, EspState& state) {
    if (!settings.esp_enabled) return;
    if (!snap.has_view) return;

    const float screen_w = static_cast<float>(overlay.width());
    const float screen_h = static_cast<float>(overlay.height());
    const rr::math::Vec2 centre = overlay.centre();

    rr::aimlogic::Filter filter;
    filter.team_check = settings.esp_team_check;
    filter.visible_only = settings.esp_visible_only;
    filter.max_world_distance = settings.esp_max_world_distance;

    for (const auto& ent : snap.entities) {
        if (!rr::aimlogic::eligible(ent, snap.local, snap.has_local, filter)) continue;
        if (ent.has_health && ent.health > state.health_scale) {
            state.health_scale = ent.health;
        }

        const float height = settings.player_height;
        const rr::math::Vec3 feet_world =
            rr::off::kPositionIsFeet ? ent.position
                                     : rr::game::raise(ent.position, -height * 0.5f);
        const rr::math::Vec3 head_world = rr::game::raise(feet_world, height);

        rr::math::Vec2 feet;
        rr::math::Vec2 head;
        if (!project(snap, feet_world, screen_w, screen_h, feet)) continue;
        if (!project(snap, head_world, screen_w, screen_h, head)) continue;

        const float box_h = feet.y - head.y;
        if (box_h < 2.0f || box_h > screen_h * 4.0f) continue;
        const float ratio = settings.player_height <= 0.0f
                                ? 0.5f
                                : settings.player_width / settings.player_height;
        const float box_w = box_h * ratio;
        if (box_w < 1.0f || box_w > screen_w) continue;
        const float box_x = feet.x - box_w * 0.5f;
        const float box_y = head.y;

        if (settings.esp_max_distance_px > 0) {
            const float from_centre = rr::math::screen_distance(feet, centre);
            if (from_centre > static_cast<float>(settings.esp_max_distance_px)) continue;
        }

        const bool teammate =
            rr::aimlogic::is_teammate(ent, snap.local, snap.has_local);
        const COLORREF colour = teammate ? rr::ui::kTeam : rr::ui::kEnemy;

        if (settings.esp_box_fill) {
            overlay.filled_rect(box_x + 1.0f, box_y + 1.0f, box_w - 2.0f, box_h - 2.0f,
                                rr::ui::kPanel);
        }
        if (settings.esp_box) {
            overlay.outlined_rect(box_x, box_y, box_w, box_h, colour);
        }

        if (settings.esp_skeleton) {
            draw_skeleton(overlay, snap, ent, screen_w, screen_h, colour);
        }

        if (settings.esp_head_dot) {
            rr::math::Vec3 head_point;
            bool have_point = rr::aimlogic::hitbox_world_point(
                ent, rr::aimlogic::Hitbox::HeadBone, height, head_point);
            if (!have_point) {
                have_point = rr::aimlogic::hitbox_world_point(
                    ent, rr::aimlogic::Hitbox::UpperPoint, height, head_point);
            }
            rr::math::Vec2 dot;
            if (have_point && project(snap, head_point, screen_w, screen_h, dot)) {
                const float radius = box_w * 0.16f;
                overlay.circle(dot.x, dot.y, radius < 2.0f ? 2.0f : radius, colour, 1);
            }
        }

        if (settings.esp_health && ent.has_health) {
            const float scale =
                static_cast<float>(state.health_scale < 1 ? 1 : state.health_scale);
            const float frac =
                rr::math::clampf(static_cast<float>(ent.health) / scale, 0.0f, 1.0f);
            const float bar_x = box_x - 6.0f;
            overlay.filled_rect(bar_x, box_y, 3.0f, box_h, rr::ui::kOutline);
            const float filled = box_h * frac;
            overlay.filled_rect(bar_x, box_y + (box_h - filled), 3.0f, filled,
                                frac > 0.5f ? rr::ui::kGood
                                            : (frac > 0.25f ? rr::ui::kWarn : rr::ui::kBad));
        }

        if (settings.esp_name && ent.has_name) {
            overlay.text_centered(feet.x, box_y - 15.0f, ent.name, rr::ui::kWhite);
        }

        if (settings.esp_distance) {
            if (settings.esp_show_world_distance && ent.has_distance) {
                overlay.text_centered(feet.x, feet.y + 2.0f,
                                      format_number(ent.distance_world, 0) + "u",
                                      rr::ui::kGrey);
            } else {
                const float from_centre = rr::math::screen_distance(feet, centre);
                overlay.text_centered(feet.x, feet.y + 2.0f,
                                      format_number(from_centre, 0) + "px", rr::ui::kGrey);
            }
        }

        if (settings.esp_snapline) {
            overlay.line(centre.x, screen_h, feet.x, feet.y, colour, 1);
        }
    }
}

} // namespace rr::feature
