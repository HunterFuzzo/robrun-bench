// External overlay: read-only attach, per-frame snapshot, GDI draw, SendInput aim.
// Chosen over an injected renderer hook because nothing is confirmed about this
// build yet -- an external process that only reads survives being wrong.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "aim.hpp"
#include "config.hpp"
#include "entity.hpp"
#include "esp.hpp"
#include "input.hpp"
#include "menu.hpp"
#include "offsets.hpp"
#include "overlay.hpp"
#include "process.hpp"
#include "settings.hpp"
#include "world.hpp"

namespace {

struct Args {
    std::string process_name;
    std::uint32_t pid = 0;
    std::string config_path = "robrun_cfg.json";
};

void print_usage() {
    std::printf(
        "rr_overlay -- external read-only overlay\n"
        "\n"
        "  rr_overlay --process <exe>     attach by executable name\n"
        "  rr_overlay --pid <n>           attach by pid\n"
        "  rr_overlay --config <path>     settings file (default robrun_cfg.json)\n"
        "\n"
        "INSERT toggles the menu, END shuts the overlay down.\n");
}

bool parse_args(int argc, char** argv, Args& out) {
    if (rr::config::kHasProcessName) out.process_name = std::string(rr::config::kProcessName);
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool has_value = (i + 1) < argc;
        if (arg == "--help" || arg == "-h") return false;
        if (arg == "--process" && has_value) {
            out.process_name = rr::win::to_lower_copy(argv[++i]);
            continue;
        }
        if (arg == "--pid" && has_value) {
            out.pid = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 10));
            continue;
        }
        if (arg == "--config" && has_value) {
            out.config_path = argv[++i];
            continue;
        }
        std::printf("unknown or incomplete argument: %s\n", arg.c_str());
        return false;
    }
    return !out.process_name.empty() || out.pid != 0;
}

double now_seconds() {
    LARGE_INTEGER freq{};
    LARGE_INTEGER counter{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    if (freq.QuadPart == 0) return 0.0;
    return static_cast<double>(counter.QuadPart) / static_cast<double>(freq.QuadPart);
}

std::string target_line(const rr::game::World& world, const Args& args) {
    char buffer[200]{};
    std::snprintf(buffer, sizeof(buffer), "%s pid %u  %s base %llX size %llX",
                  args.process_name.empty() ? "pid-attach" : args.process_name.c_str(),
                  world.pid(), world.main_module().name.c_str(),
                  static_cast<unsigned long long>(world.main_module().base),
                  static_cast<unsigned long long>(world.main_module().size));
    return buffer;
}

std::string build_line() {
    if (rr::off::kCanEsp) return "offsets present -- verify them against the running build";
    return "offsets unset -- fill src/offsets.hpp from the probe report";
}

} // namespace

int main(int argc, char** argv) {
    Args args;
    if (!parse_args(argc, argv, args)) {
        print_usage();
        return 2;
    }

    rr::game::World world;
    const bool attached = args.pid != 0 ? world.attach_pid(args.pid)
                                        : world.attach(args.process_name);
    if (!attached) {
        std::printf("attach failed (process missing, or run this elevated)\n");
        return 3;
    }
    std::printf("attached  %s\n", target_line(world, args).c_str());
    for (const auto& line : world.gate_report()) {
        std::printf("gated     %s\n", line.c_str());
    }

    std::printf("note      windowed or borderless draws fine, exclusive fullscreen hides "
                "the overlay\n");

    HWND target = rr::ui::find_window_for_pid(static_cast<DWORD>(world.pid()));
    if (target == nullptr) {
        std::printf("no visible window for that pid yet; waiting\n");
    }

    rr::ui::Overlay overlay;
    if (!overlay.create(L"rr_overlay_layer")) {
        std::printf("overlay creation failed\n");
        return 4;
    }

    rr::cfg::Settings settings;
    rr::ui::MenuState menu;
    rr::ui::MenuInfo info;
    rr::input::Input input;
    rr::feature::EspState esp_state;
    rr::feature::Aim aim;
    rr::game::Snapshot snapshot;

    info.config_path = args.config_path;
    info.target_line = target_line(world, args);
    info.build_line = build_line();
    info.gate_lines = world.gate_report();
    if (rr::cfg::load(settings, args.config_path)) {
        info.status_line = "loaded " + args.config_path;
    }

    double last_frame = now_seconds();
    double fps_accum = 0.0;
    int fps_frames = 0;
    bool running = true;

    while (running) {
        const double frame_start = now_seconds();
        const double delta = frame_start - last_frame;
        last_frame = frame_start;
        if (delta > 0.0) {
            fps_accum += delta;
            ++fps_frames;
            if (fps_accum >= 0.25) {
                info.fps = static_cast<float>(fps_frames) / static_cast<float>(fps_accum);
                fps_accum = 0.0;
                fps_frames = 0;
            }
        }

        overlay.pump_messages();
        input.poll();

        if (input.pressed(settings.panic_key)) {
            running = false;
        }
        if (input.pressed(settings.menu_key)) {
            menu.open = !menu.open;
            overlay.set_click_through(!menu.open);
        }

        if (!world.alive()) {
            std::printf("target exited\n");
            running = false;
        }

        if (target == nullptr || !IsWindow(target)) {
            target = rr::ui::find_window_for_pid(static_cast<DWORD>(world.pid()));
        }
        // A minimised or zero-sized target cannot be followed; drawing a stale
        // buffer over the desktop is worse than drawing nothing.
        bool following = false;
        if (target != nullptr) {
            following = overlay.follow(target);
        }

        const HWND foreground = GetForegroundWindow();
        const bool target_focused = (target != nullptr) && (foreground == target);
        const bool overlay_focused = foreground == overlay.hwnd();
        const bool visible = following && (target_focused || (menu.open && overlay_focused));

        world.snapshot(snapshot);
        info.entity_count = snapshot.entities.size();
        info.rejected = snapshot.rejected;
        info.bones_per_entity = 0;
        for (const auto& entity : snapshot.entities) {
            if (!entity.bones.empty()) {
                info.bones_per_entity = entity.bones.size();
                break;
            }
        }

        overlay.begin_frame();
        if (visible) {
            if (settings.show_watermark) {
                overlay.text(8.0f, 6.0f, "robrun bench", rr::ui::kAccent);
            }
            if (settings.show_fps) {
                char fps_text[32]{};
                std::snprintf(fps_text, sizeof(fps_text), "%.0f fps",
                              static_cast<double>(info.fps));
                overlay.text(8.0f, 22.0f, fps_text, rr::ui::kGrey);
            }
            if (settings.show_crosshair) {
                const rr::math::Vec2 centre = overlay.centre();
                overlay.line(centre.x - 7.0f, centre.y, centre.x + 7.0f, centre.y,
                             rr::ui::kAccent, 1);
                overlay.line(centre.x, centre.y - 7.0f, centre.x, centre.y + 7.0f,
                             rr::ui::kAccent, 1);
            }
            rr::feature::draw_esp(overlay, snapshot, settings, esp_state);
        }

        aim.update(overlay, snapshot, settings, input, target_focused, menu.open);
        rr::ui::draw_menu(overlay, input, menu, settings, info);
        overlay.end_frame();

        if (menu.save_requested) {
            menu.save_requested = false;
            info.status_line = rr::cfg::save(settings, args.config_path)
                                   ? "saved " + args.config_path
                                   : "save failed: " + args.config_path;
        }
        if (menu.load_requested) {
            menu.load_requested = false;
            info.status_line = rr::cfg::load(settings, args.config_path)
                                   ? "loaded " + args.config_path
                                   : "load failed: " + args.config_path;
        }

        const int cap = settings.frame_cap < 30 ? 30 : settings.frame_cap;
        const double budget = 1.0 / static_cast<double>(cap);
        const double spent = now_seconds() - frame_start;
        if (spent < budget) {
            const DWORD sleep_ms = static_cast<DWORD>((budget - spent) * 1000.0);
            Sleep(sleep_ms > 0 ? sleep_ms : 1);
        }
    }

    aim.release();
    overlay.destroy();
    std::printf("stopped\n");
    return 0;
}
