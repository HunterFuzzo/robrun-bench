#pragma once
// Reads the game side into a per-frame snapshot. Every read is exact-or-fail and
// every value is checked for plausibility before it reaches the renderer.
//
// Netcode assumption: this reads whatever the client already has in memory, which
// is the interpolated snapshot the game itself is drawing. Tick rate, delta
// compression and interpolation delay are not modelled, so an entity that the
// client is extrapolating will read slightly behind where it is drawn.
#include <cstdint>
#include <string>
#include <vector>

#include "entity.hpp"
#include "process.hpp"

namespace rr::game {

class World {
public:
    bool attach(const std::string& exe_name_lower);
    bool attach_pid(std::uint32_t pid);
    [[nodiscard]] bool attached() const { return attached_; }
    [[nodiscard]] bool alive() const { return proc_.alive(); }
    [[nodiscard]] const rr::win::ModuleInfo& main_module() const { return main_; }
    [[nodiscard]] std::uint32_t pid() const { return pid_; }

    // One line per feature that is switched off, with the reason.
    [[nodiscard]] std::vector<std::string> gate_report() const;

    // Rebuilds the snapshot from scratch. False means the process went away or the
    // entity list could not be read at all.
    bool snapshot(Snapshot& out);

private:
    bool finish_attach();
    [[nodiscard]] bool read_entity(std::uint64_t address, Entity& out) const;
    [[nodiscard]] std::size_t read_count() const;

    rr::win::Process proc_;
    rr::win::ModuleInfo main_{};
    std::uint32_t pid_ = 0;
    bool attached_ = false;
    std::uint64_t frame_ = 0;
};

} // namespace rr::game
