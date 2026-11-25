#include "menu.hpp"

#include <cstdio>
#include <string>

#include "math.hpp"
#include "offsets.hpp"
#include "target.hpp"

namespace rr::ui {
namespace {

constexpr float kPanelWidth = 430.0f;
constexpr float kPanelHeight = 420.0f;
constexpr float kTitleHeight = 26.0f;
constexpr float kTabHeight = 24.0f;
constexpr float kPad = 12.0f;
constexpr float kRow = 23.0f;
constexpr float kBoxSize = 13.0f;
constexpr float kSliderWidth = 150.0f;
constexpr int kTabCount = 4;
const char* const kTabNames[kTabCount] = {"ESP", "AIMBOT", "MISC", "CONFIG"};

std::string format_float(float value, int decimals) {
    char buffer[48]{};
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, static_cast<double>(value));
    return buffer;
}

std::string format_int(int value) {
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%d", value);
    return buffer;
}

// Layout and hit-testing for one frame of the menu.
class Builder {
public:
    Builder(Overlay& overlay, const rr::input::Input& input, MenuState& state)
        : overlay_(overlay), input_(input), state_(state) {
        const rr::math::Vec2 screen = input.cursor();
        const rr::math::Vec2 origin = overlay.origin();
        mouse_ = rr::math::Vec2{screen.x - origin.x, screen.y - origin.y};
        mouse_down_ = input.down(VK_LBUTTON);
        mouse_pressed_ = input.pressed(VK_LBUTTON);
    }

    void begin_content(float x, float y) {
        content_x_ = x;
        cursor_y_ = y;
        widget_id_ = 0;
    }

    [[nodiscard]] rr::math::Vec2 mouse() const { return mouse_; }
    [[nodiscard]] bool mouse_down() const { return mouse_down_; }
    [[nodiscard]] bool mouse_pressed() const { return mouse_pressed_; }

    bool hovering(float x, float y, float w, float h) const {
        return mouse_.x >= x && mouse_.x <= x + w && mouse_.y >= y && mouse_.y <= y + h;
    }

    void label(const std::string& text, COLORREF colour = kGrey) {
        overlay_.text(content_x_, cursor_y_, text, colour);
        cursor_y_ += kRow;
    }

    void gap(float amount) { cursor_y_ += amount; }

    void checkbox(const std::string& text, bool& value, bool enabled = true) {
        const int id = widget_id_++;
        const float y = cursor_y_;
        const bool hot = enabled && hovering(content_x_, y, kPanelWidth - kPad * 2.0f, kRow);
        overlay_.filled_rect(content_x_, y + 2.0f, kBoxSize, kBoxSize,
                             value ? kAccent : kPanel);
        overlay_.rect(content_x_, y + 2.0f, kBoxSize, kBoxSize,
                      hot ? kWhite : kGrey, 1);
        overlay_.text(content_x_ + kBoxSize + 8.0f, y, text,
                      enabled ? (value ? kWhite : kGrey) : RGB(90, 90, 94));
        if (hot && mouse_pressed_) value = !value;
        (void)id;
        cursor_y_ += kRow;
    }

    void slider_int(const std::string& text, int& value, int min_value, int max_value) {
        const int id = widget_id_++;
        const float y = cursor_y_;
        const float track_x = content_x_ + kPanelWidth - kPad * 2.0f - kSliderWidth;
        overlay_.text(content_x_, y, text, kGrey);
        drag_track(id, track_x, y, static_cast<float>(min_value),
                   static_cast<float>(max_value), value_as_float(value));
        const float span = static_cast<float>(max_value - min_value);
        const float frac = span <= 0.0f
                               ? 0.0f
                               : (static_cast<float>(value - min_value) / span);
        paint_track(track_x, y, frac);
        if (state_.active_slider == id && mouse_down_) {
            const float raw = (mouse_.x - track_x) / kSliderWidth;
            const float clamped = rr::math::clampf(raw, 0.0f, 1.0f);
            value = min_value + static_cast<int>(clamped * span + 0.5f);
        }
        overlay_.text(track_x + kSliderWidth + 8.0f, y, format_int(value), kWhite);
        cursor_y_ += kRow;
    }

    void slider_float(const std::string& text, float& value, float min_value,
                      float max_value, int decimals) {
        const int id = widget_id_++;
        const float y = cursor_y_;
        const float track_x = content_x_ + kPanelWidth - kPad * 2.0f - kSliderWidth;
        overlay_.text(content_x_, y, text, kGrey);
        drag_track(id, track_x, y, min_value, max_value, value);
        const float span = max_value - min_value;
        const float frac = span <= 0.0f ? 0.0f : (value - min_value) / span;
        paint_track(track_x, y, rr::math::clampf(frac, 0.0f, 1.0f));
        if (state_.active_slider == id && mouse_down_) {
            const float raw = (mouse_.x - track_x) / kSliderWidth;
            value = min_value + rr::math::clampf(raw, 0.0f, 1.0f) * span;
        }
        overlay_.text(track_x + kSliderWidth + 8.0f, y, format_float(value, decimals),
                      kWhite);
        cursor_y_ += kRow;
    }

    void keybind(const std::string& text, int& value) {
        const int id = widget_id_++;
        const float y = cursor_y_;
        const float box_w = 86.0f;
        const float box_x = content_x_ + kPanelWidth - kPad * 2.0f - box_w;
        const bool capturing = state_.capture_id == id;
        overlay_.text(content_x_, y, text, kGrey);
        overlay_.filled_rect(box_x, y + 1.0f, box_w, kRow - 5.0f, kPanel);
        overlay_.rect(box_x, y + 1.0f, box_w, kRow - 5.0f,
                      capturing ? kAccent : kGrey, 1);
        const std::string shown =
            capturing ? std::string("...") : std::string(rr::input::key_name(value));
        overlay_.text_centered(box_x + box_w * 0.5f, y, shown,
                               capturing ? kAccent : kWhite);
        if (hovering(box_x, y, box_w, kRow) && mouse_pressed_ && !capturing) {
            state_.capture_id = id;
            state_.capture_armed = false;
        }
        if (capturing) {
            if (!state_.capture_armed) {
                // Skip the frame that opened the capture so the click itself is not bound.
                if (!mouse_down_) state_.capture_armed = true;
            } else {
                const int key = input_.first_pressed();
                if (key != 0) {
                    value = key;
                    state_.capture_id = -1;
                    state_.capture_armed = false;
                }
            }
        }
        cursor_y_ += kRow;
    }

    void combo(const std::string& text, int& value, const char* const* names, int count,
               const std::string& suffix) {
        widget_id_++;
        const float y = cursor_y_;
        const float box_w = 150.0f;
        const float box_x = content_x_ + kPanelWidth - kPad * 2.0f - box_w;
        overlay_.text(content_x_, y, text, kGrey);
        overlay_.filled_rect(box_x, y + 1.0f, box_w, kRow - 5.0f, kPanel);
        overlay_.rect(box_x, y + 1.0f, box_w, kRow - 5.0f, kGrey, 1);
        overlay_.text(box_x + 4.0f, y, "<", kAccent);
        overlay_.text(box_x + box_w - 12.0f, y, ">", kAccent);
        const int clamped = rr::math::clampi(value, 0, count - 1);
        const std::string shown =
            std::string(names[clamped]) + (suffix.empty() ? "" : " " + suffix);
        overlay_.text_centered(box_x + box_w * 0.5f, y, shown, kWhite);
        if (mouse_pressed_ && hovering(box_x, y, box_w * 0.5f, kRow)) {
            value = (clamped + count - 1) % count;
        } else if (mouse_pressed_ && hovering(box_x + box_w * 0.5f, y, box_w * 0.5f, kRow)) {
            value = (clamped + 1) % count;
        } else {
            value = clamped;
        }
        cursor_y_ += kRow;
    }

    bool button(const std::string& text, float width) {
        widget_id_++;
        const float y = cursor_y_;
        const bool hot = hovering(content_x_, y, width, kRow - 4.0f);
        overlay_.filled_rect(content_x_, y, width, kRow - 4.0f, hot ? kAccent : kPanel);
        overlay_.rect(content_x_, y, width, kRow - 4.0f, kGrey, 1);
        overlay_.text_centered(content_x_ + width * 0.5f, y + 1.0f, text,
                               hot ? kDark : kWhite);
        cursor_y_ += kRow;
        return hot && mouse_pressed_;
    }

    [[nodiscard]] float cursor_y() const { return cursor_y_; }

private:
    static float value_as_float(int value) { return static_cast<float>(value); }

    void drag_track(int id, float track_x, float y, float, float, float) {
        if (hovering(track_x, y, kSliderWidth, kRow) && mouse_pressed_) {
            state_.active_slider = id;
        }
        if (!mouse_down_ && state_.active_slider == id) {
            state_.active_slider = -1;
        }
    }

    void paint_track(float track_x, float y, float frac) {
        const float track_y = y + 6.0f;
        overlay_.filled_rect(track_x, track_y, kSliderWidth, 5.0f, kPanel);
        overlay_.filled_rect(track_x, track_y, kSliderWidth * frac, 5.0f, kAccent);
        overlay_.rect(track_x, track_y, kSliderWidth, 5.0f, kGrey, 1);
    }

    Overlay& overlay_;
    const rr::input::Input& input_;
    MenuState& state_;
    rr::math::Vec2 mouse_;
    bool mouse_down_ = false;
    bool mouse_pressed_ = false;
    float content_x_ = 0.0f;
    float cursor_y_ = 0.0f;
    int widget_id_ = 0;
};

void draw_esp_tab(Builder& ui, rr::cfg::Settings& s) {
    ui.checkbox("esp master", s.esp_enabled);
    ui.checkbox("box", s.esp_box);
    ui.checkbox("box fill", s.esp_box_fill);
    ui.checkbox("skeleton", s.esp_skeleton, rr::off::kCanSkeleton);
    ui.checkbox("head dot", s.esp_head_dot);
    ui.checkbox("health bar", s.esp_health);
    ui.checkbox("name", s.esp_name);
    ui.checkbox("snapline", s.esp_snapline);
    ui.checkbox("distance", s.esp_distance);
    ui.checkbox("distance in world units", s.esp_show_world_distance,
                rr::off::kCanWorldDistance);
    ui.checkbox("team check", s.esp_team_check);
    ui.checkbox("visible only", s.esp_visible_only, rr::off::kCanVisibilityFilter);
    ui.slider_int("screen cull px (0 = off)", s.esp_max_distance_px, 0, 2000);
    ui.slider_float("world cull (0 = off)", s.esp_max_world_distance, 0.0f, 10000.0f, 0);
}

void draw_aim_tab(Builder& ui, rr::cfg::Settings& s) {
    static const char* const kHitboxNames[] = {"body", "upper", "head bone", "chest bone",
                                               "nearest bone"};
    constexpr int kHitboxCount = static_cast<int>(rr::aimlogic::Hitbox::Count);
    const auto wanted = rr::aimlogic::hitbox_from_int(s.aim_hitbox);
    const auto resolved = rr::aimlogic::resolve_hitbox(wanted);
    const std::string suffix =
        (resolved == wanted) ? std::string()
                             : std::string("-> ") + rr::aimlogic::hitbox_name(resolved);

    ui.checkbox("aim master", s.aim_enabled);
    ui.keybind("aim key", s.aim_key);
    ui.combo("hitbox", s.aim_hitbox, kHitboxNames, kHitboxCount, suffix);
    ui.slider_int("fov radius px", s.aim_fov_px, 1, 1000);
    ui.checkbox("draw fov circle", s.aim_draw_fov);
    ui.slider_float("smoothing", s.aim_smoothing, 1.0f, 40.0f, 1);
    ui.slider_float("px -> counts (uncal)", s.aim_pixel_scale, 0.05f, 4.0f, 2);
    ui.checkbox("sticky target", s.aim_sticky);
    ui.checkbox("team check", s.aim_team_check);
    ui.checkbox("visible only", s.aim_visible_only, rr::off::kCanVisibilityFilter);
    ui.slider_float("world cull (0 = off)", s.aim_max_world_distance, 0.0f, 10000.0f, 0);
    ui.checkbox("trigger", s.trigger_enabled);
    ui.slider_int("trigger fov px", s.trigger_fov_px, 1, 60);
    ui.slider_int("trigger delay ms", s.trigger_delay_ms, 0, 300);
}

void draw_misc_tab(Builder& ui, rr::cfg::Settings& s) {
    ui.checkbox("crosshair", s.show_crosshair);
    ui.checkbox("fps counter", s.show_fps);
    ui.checkbox("watermark", s.show_watermark);
    ui.checkbox("line to target", s.show_target_line);
    ui.slider_float("player height (uncal)", s.player_height, 1.0f, 400.0f, 1);
    ui.slider_float("player width (uncal)", s.player_width, 1.0f, 200.0f, 1);
    ui.slider_int("frame cap", s.frame_cap, 30, 360);
    ui.keybind("menu key", s.menu_key);
    ui.keybind("panic key", s.panic_key);
}

void draw_config_tab(Builder& ui, MenuState& state, const MenuInfo& info) {
    ui.label(info.target_line, kWhite);
    ui.label(info.build_line, kWarn);
    char line[160]{};
    std::snprintf(line, sizeof(line), "entities %zu  dropped %zu  fps %.0f",
                  info.entity_count, info.rejected, static_cast<double>(info.fps));
    ui.label(line, kGrey);
    ui.gap(4.0f);

    const std::size_t shown = info.gate_lines.size() < 6 ? info.gate_lines.size() : 6;
    for (std::size_t i = 0; i < shown; ++i) {
        ui.label("- " + info.gate_lines[i], kBad);
    }
    if (info.gate_lines.size() > shown) {
        ui.label("- " + format_int(static_cast<int>(info.gate_lines.size() - shown)) +
                     " more gated",
                 kBad);
    }
    if (info.gate_lines.empty()) {
        ui.label("all features have their offsets", kGood);
    }
    ui.gap(4.0f);
    if (ui.button("save " + info.config_path, 180.0f)) state.save_requested = true;
    if (ui.button("load " + info.config_path, 180.0f)) state.load_requested = true;
    if (!info.status_line.empty()) ui.label(info.status_line, kGrey);
}

} // namespace

void draw_menu(Overlay& overlay, const rr::input::Input& input, MenuState& state,
               rr::cfg::Settings& settings, const MenuInfo& info) {
    if (!state.open) {
        state.capture_id = -1;
        state.active_slider = -1;
        state.dragging = false;
        return;
    }

    Builder ui(overlay, input, state);

    // Keep the panel inside the overlay even after a resize.
    const float max_x = static_cast<float>(overlay.width()) - kPanelWidth;
    const float max_y = static_cast<float>(overlay.height()) - kPanelHeight;
    state.x = rr::math::clampf(state.x, 0.0f, max_x < 0.0f ? 0.0f : max_x);
    state.y = rr::math::clampf(state.y, 0.0f, max_y < 0.0f ? 0.0f : max_y);

    const float x = state.x;
    const float y = state.y;
    const rr::math::Vec2 mouse = ui.mouse();

    if (ui.mouse_pressed() && ui.hovering(x, y, kPanelWidth, kTitleHeight)) {
        state.dragging = true;
        state.drag_dx = mouse.x - x;
        state.drag_dy = mouse.y - y;
    }
    if (!ui.mouse_down()) state.dragging = false;
    if (state.dragging) {
        state.x = mouse.x - state.drag_dx;
        state.y = mouse.y - state.drag_dy;
    }

    overlay.filled_rect(x, y, kPanelWidth, kPanelHeight, kDark);
    overlay.rect(x, y, kPanelWidth, kPanelHeight, kAccent, 1);
    overlay.filled_rect(x, y, kPanelWidth, kTitleHeight, kPanel);
    overlay.text(x + kPad, y + 5.0f, "robrun bench", kAccent, false);
    overlay.text(x + kPanelWidth - 118.0f, y + 6.0f, "insert to hide", kGrey);

    const float tab_width = kPanelWidth / static_cast<float>(kTabCount);
    for (int i = 0; i < kTabCount; ++i) {
        const float tab_x = x + static_cast<float>(i) * tab_width;
        const float tab_y = y + kTitleHeight;
        const bool active = state.tab == i;
        const bool hot = ui.hovering(tab_x, tab_y, tab_width, kTabHeight);
        overlay.filled_rect(tab_x, tab_y, tab_width, kTabHeight, active ? kDark : kPanel);
        overlay.text_centered(tab_x + tab_width * 0.5f, tab_y + 4.0f, kTabNames[i],
                              active ? kAccent : (hot ? kWhite : kGrey));
        if (active) {
            overlay.filled_rect(tab_x, tab_y + kTabHeight - 2.0f, tab_width, 2.0f, kAccent);
        }
        if (hot && ui.mouse_pressed()) state.tab = i;
    }

    ui.begin_content(x + kPad, y + kTitleHeight + kTabHeight + kPad);
    switch (state.tab) {
        case 0: draw_esp_tab(ui, settings); break;
        case 1: draw_aim_tab(ui, settings); break;
        case 2: draw_misc_tab(ui, settings); break;
        case 3: draw_config_tab(ui, state, info); break;
        default: state.tab = 0; break;
    }
}

} // namespace rr::ui
