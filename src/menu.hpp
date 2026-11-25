#pragma once
// Immediate-mode menu drawn into the overlay's back buffer. Tabbed, draggable by
// its title bar, no retained widget tree: every widget is hit-tested against the
// polled cursor for the frame it is drawn in.
#include <string>
#include <vector>

#include "input.hpp"
#include "overlay.hpp"
#include "settings.hpp"

namespace rr::ui {

struct MenuState {
    bool open = false;
    float x = 140.0f;
    float y = 140.0f;
    int tab = 0;
    bool dragging = false;
    float drag_dx = 0.0f;
    float drag_dy = 0.0f;
    int capture_id = -1;      // widget id currently waiting for a key
    bool capture_armed = false;
    int active_slider = -1;   // widget id being dragged
    bool save_requested = false;
    bool load_requested = false;
};

struct MenuInfo {
    std::string target_line;              // process, pid, module, image size
    std::string build_line;               // offsets state in one line
    std::vector<std::string> gate_lines;  // one per disabled feature
    std::string config_path;
    std::string status_line;              // result of the last save or load
    float fps = 0.0f;
    std::size_t entity_count = 0;
    std::size_t rejected = 0;
    std::size_t bones_per_entity = 0;     // slots read on the first entity that has any
};

// Draws the menu when state.open is true. Consumes the clicks it handles.
void draw_menu(Overlay& overlay, const rr::input::Input& input, MenuState& state,
               rr::cfg::Settings& settings, const MenuInfo& info);

} // namespace rr::ui
