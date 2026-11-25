#include "settings.hpp"

#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>
#include <string_view>

namespace rr::cfg {
namespace {

struct BoolField {
    const char* key;
    bool Settings::*member;
};

struct IntField {
    const char* key;
    int Settings::*member;
};

struct FloatField {
    const char* key;
    float Settings::*member;
};

constexpr BoolField kBools[] = {
    {"esp_enabled", &Settings::esp_enabled},
    {"esp_box", &Settings::esp_box},
    {"esp_health", &Settings::esp_health},
    {"esp_name", &Settings::esp_name},
    {"esp_snapline", &Settings::esp_snapline},
    {"esp_distance", &Settings::esp_distance},
    {"esp_team_check", &Settings::esp_team_check},
    {"esp_skeleton", &Settings::esp_skeleton},
    {"esp_head_dot", &Settings::esp_head_dot},
    {"esp_box_fill", &Settings::esp_box_fill},
    {"esp_visible_only", &Settings::esp_visible_only},
    {"esp_bone_debug", &Settings::esp_bone_debug},
    {"esp_bone_debug_nearest", &Settings::esp_bone_debug_nearest},
    {"esp_show_world_distance", &Settings::esp_show_world_distance},
    {"aim_enabled", &Settings::aim_enabled},
    {"aim_team_check", &Settings::aim_team_check},
    {"aim_visible_only", &Settings::aim_visible_only},
    {"aim_sticky", &Settings::aim_sticky},
    {"aim_draw_fov", &Settings::aim_draw_fov},
    {"trigger_enabled", &Settings::trigger_enabled},
    {"show_crosshair", &Settings::show_crosshair},
    {"show_fps", &Settings::show_fps},
    {"show_watermark", &Settings::show_watermark},
    {"show_target_line", &Settings::show_target_line},
};

constexpr IntField kInts[] = {
    {"esp_max_distance_px", &Settings::esp_max_distance_px},
    {"esp_bone_debug_index", &Settings::esp_bone_debug_index},
    {"aim_key", &Settings::aim_key},
    {"aim_hitbox", &Settings::aim_hitbox},
    {"aim_fov_px", &Settings::aim_fov_px},
    {"trigger_fov_px", &Settings::trigger_fov_px},
    {"trigger_delay_ms", &Settings::trigger_delay_ms},
    {"frame_cap", &Settings::frame_cap},
    {"menu_key", &Settings::menu_key},
    {"panic_key", &Settings::panic_key},
};

constexpr FloatField kFloats[] = {
    {"esp_max_world_distance", &Settings::esp_max_world_distance},
    {"aim_max_world_distance", &Settings::aim_max_world_distance},
    {"aim_smoothing", &Settings::aim_smoothing},
    {"aim_pixel_scale", &Settings::aim_pixel_scale},
    {"player_height", &Settings::player_height},
    {"player_width", &Settings::player_width},
};

std::string trim(std::string_view in) {
    std::size_t a = 0;
    std::size_t b = in.size();
    const auto space = [](char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',';
    };
    while (a < b && space(in[a])) ++a;
    while (b > a && space(in[b - 1])) --b;
    return std::string(in.substr(a, b - a));
}

std::string strip_quotes(std::string in) {
    if (in.size() >= 2 && in.front() == '"' && in.back() == '"') {
        return in.substr(1, in.size() - 2);
    }
    return in;
}

} // namespace

std::string to_json(const Settings& s) {
    std::string out = "{\n";
    char line[160]{};
    for (const auto& f : kBools) {
        std::snprintf(line, sizeof(line), "  \"%s\": %s,\n", f.key,
                      (s.*f.member) ? "true" : "false");
        out += line;
    }
    for (const auto& f : kInts) {
        std::snprintf(line, sizeof(line), "  \"%s\": %d,\n", f.key, s.*f.member);
        out += line;
    }
    for (std::size_t i = 0; i < std::size(kFloats); ++i) {
        const auto& f = kFloats[i];
        std::snprintf(line, sizeof(line), "  \"%s\": %.4f%s\n", f.key,
                      static_cast<double>(s.*f.member),
                      (i + 1 < std::size(kFloats)) ? "," : "");
        out += line;
    }
    out += "}\n";
    return out;
}

bool save(const Settings& s, const std::string& path) {
    const std::string json = to_json(s);
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) return false;
    const std::size_t written = std::fwrite(json.data(), 1, json.size(), f);
    std::fclose(f);
    return written == json.size();
}

bool load(Settings& s, const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return false;
    std::string text;
    char chunk[512]{};
    std::size_t got = 0;
    while ((got = std::fread(chunk, 1, sizeof(chunk), f)) > 0) {
        text.append(chunk, got);
    }
    std::fclose(f);
    if (text.empty()) return false;

    bool any = false;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t eol = text.find('\n', pos);
        const std::string_view raw(text.data() + pos,
                                   (eol == std::string::npos ? text.size() : eol) - pos);
        pos = (eol == std::string::npos) ? text.size() : eol + 1;

        const std::size_t colon = raw.find(':');
        if (colon == std::string_view::npos) continue;
        const std::string key = strip_quotes(trim(raw.substr(0, colon)));
        const std::string value = strip_quotes(trim(raw.substr(colon + 1)));
        if (key.empty() || value.empty()) continue;

        bool matched = false;
        for (const auto& fld : kBools) {
            if (key == fld.key) {
                s.*fld.member = (value == "true" || value == "1");
                matched = true;
                break;
            }
        }
        if (matched) {
            any = true;
            continue;
        }
        for (const auto& fld : kInts) {
            if (key == fld.key) {
                s.*fld.member = static_cast<int>(std::strtol(value.c_str(), nullptr, 0));
                matched = true;
                break;
            }
        }
        if (matched) {
            any = true;
            continue;
        }
        for (const auto& fld : kFloats) {
            if (key == fld.key) {
                s.*fld.member = std::strtof(value.c_str(), nullptr);
                matched = true;
                break;
            }
        }
        if (matched) any = true;
    }
    return any;
}

} // namespace rr::cfg
