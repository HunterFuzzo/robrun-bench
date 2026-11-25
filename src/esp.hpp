#pragma once
// World-space drawing. Nothing here reads the game; it consumes a snapshot.
#include <cstdint>

#include "entity.hpp"
#include "overlay.hpp"
#include "settings.hpp"

namespace rr::feature {

// Health has no confirmed maximum on this build, so the bar scales against the
// largest value seen this session instead of an invented 100.
struct EspState {
    std::int32_t health_scale = 1;
};

void draw_esp(rr::ui::Overlay& overlay, const rr::game::Snapshot& snap,
              const rr::cfg::Settings& settings, EspState& state);

} // namespace rr::feature
