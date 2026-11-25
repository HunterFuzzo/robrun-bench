#pragma once
// Runtime toggles and their JSON persistence. Nothing build-dependent lives here;
// game values belong in offsets.hpp.
#include <string>

namespace rr::cfg {

struct Settings {
    // ESP
    bool esp_enabled = true;
    bool esp_box = true;
    bool esp_health = true;
    bool esp_name = false;
    bool esp_snapline = false;
    bool esp_distance = false;
    bool esp_team_check = true;
    bool esp_skeleton = false;
    bool esp_head_dot = false;
    bool esp_box_fill = false;
    bool esp_visible_only = false;
    bool esp_show_world_distance = false;
    int esp_max_distance_px = 0;          // 0 = no screen-distance cull
    float esp_max_world_distance = 0.0f;  // 0 = no world-distance cull

    // AIMBOT
    bool aim_enabled = false;
    int aim_key = 0x02;                   // VK_RBUTTON
    int aim_fov_px = 120;                 // radius from the crosshair, pixels
    float aim_smoothing = 6.0f;           // 1 = snap
    float aim_pixel_scale = 1.0f;         // pixels -> mouse counts, UNCALIBRATED
    int aim_hitbox = 1;                   // rr::aimlogic::Hitbox, 1 = upper point
    bool aim_team_check = true;
    bool aim_visible_only = false;
    bool aim_sticky = true;               // keeps the target while the key is held
    bool aim_draw_fov = false;
    float aim_max_world_distance = 0.0f;  // 0 = no world-distance cull
    bool trigger_enabled = false;
    int trigger_fov_px = 6;
    int trigger_delay_ms = 0;

    // MISC
    bool show_crosshair = false;
    bool show_fps = true;
    bool show_watermark = true;
    bool show_target_line = false;
    float player_height = 72.0f;          // world units, UNCALIBRATED
    float player_width = 24.0f;           // world units, UNCALIBRATED
    int frame_cap = 144;

    // keys
    int menu_key = 0x2D;                  // VK_INSERT
    int panic_key = 0x23;                 // VK_END
};

// Flat JSON, one key per line. Unknown keys are ignored; a malformed file leaves
// the defaults in place and reports false.
bool save(const Settings& s, const std::string& path);
bool load(Settings& s, const std::string& path);
std::string to_json(const Settings& s);

} // namespace rr::cfg
