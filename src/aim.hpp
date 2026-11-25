#pragma once
// Screen-space aim. The target process is never written to: the only output is a
// relative SendInput mouse move, so no view-angle offset and no write handle is
// needed. Pixel-to-counts scale is uncalibrated, and neither network
// interpolation nor input latency is modelled -- the point aimed at is where the
// client last drew the entity, not where the server will place it.
#include <cstdint>

#include "entity.hpp"
#include "input.hpp"
#include "overlay.hpp"
#include "settings.hpp"

namespace rr::feature {

struct AimState {
    bool has_target = false;
    rr::math::Vec2 target_screen;
    std::uint64_t target_address = 0;
    std::uint64_t locked_address = 0;   // sticky target while the key is held
    float target_distance_px = 0.0f;
    bool firing = false;
    std::uint64_t trigger_ready_tick = 0;
};

class Aim {
public:
    // target_focused must be the live foreground check for this frame.
    void update(rr::ui::Overlay& overlay, const rr::game::Snapshot& snap,
                const rr::cfg::Settings& settings, const rr::input::Input& input,
                bool target_focused, bool menu_open);

    // Releases anything held. Safe to call every frame and on shutdown.
    void release();

    [[nodiscard]] const AimState& state() const { return state_; }

private:
    AimState state_;
};

} // namespace rr::feature
