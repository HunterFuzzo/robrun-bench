#include "world.hpp"

#include <algorithm>
#include <limits>

#include "config.hpp"
#include "offsets.hpp"

namespace rr::game {
namespace {

// Marks a bone slot that could not be read, without shifting the ones after it.
const float kBadCoord = std::numeric_limits<float>::quiet_NaN();

// A health value outside this range is a failed read dressed up as a number.
constexpr std::int32_t kHealthMin = 1;
constexpr std::int32_t kHealthMax = 1000000;

std::string sanitize_name(const char* raw, std::size_t max_length) {
    std::string out;
    out.reserve(max_length);
    for (std::size_t i = 0; i < max_length; ++i) {
        const unsigned char c = static_cast<unsigned char>(raw[i]);
        if (c == 0) break;
        if (c < 0x20 || c > 0x7E) continue;
        out.push_back(static_cast<char>(c));
    }
    return out;
}

} // namespace

bool World::attach(const std::string& exe_name_lower) {
    attached_ = false;
    const auto found = rr::win::find_process(exe_name_lower);
    if (!found.has_value()) return false;
    pid_ = static_cast<std::uint32_t>(*found);
    if (!proc_.open(static_cast<DWORD>(pid_))) return false;
    return finish_attach();
}

bool World::attach_pid(std::uint32_t pid) {
    attached_ = false;
    pid_ = pid;
    if (!proc_.open(static_cast<DWORD>(pid))) return false;
    return finish_attach();
}

bool World::finish_attach() {
    const auto mods = proc_.modules();
    if (mods.empty()) return false;
    main_ = mods.front();
    if (main_.base == 0 || main_.size == 0) return false;
    attached_ = true;
    frame_ = 0;
    return true;
}

std::vector<std::string> World::gate_report() const {
    std::vector<std::string> out;
    if (!rr::off::kHasViewMatrix) {
        out.emplace_back("view matrix unset -- world-to-screen off, nothing draws in world "
                         "space");
    }
    if (!rr::off::kHasEntityList) {
        out.emplace_back("entity list unset -- enumeration off");
    }
    if (!rr::off::kHasPosition) {
        out.emplace_back("entity position unset -- enumeration off");
    }
    if (!rr::off::kHasHealth) {
        out.emplace_back("entity health unset -- health bar off, alive filter off");
    }
    if (!rr::off::kHasTeam) {
        out.emplace_back("entity team unset -- team check off, every entity is a target");
    }
    if (!rr::off::kHasName) {
        out.emplace_back("entity name unset -- name text off");
    }
    if (!rr::off::kHasHeadPosition) {
        out.emplace_back("head position unset -- the upper hitbox is derived from the "
                         "uncalibrated player height");
    }
    if (!rr::off::kHasVisible) {
        out.emplace_back("visible flag unset -- visible-only filters pass everything");
    }
    if (!rr::off::kHasBones) {
        out.emplace_back("bone array unset -- skeleton off, bone hitboxes fall back");
    } else if (!rr::off::kHasBoneChain) {
        out.emplace_back("bone chain empty -- bones read but no skeleton is drawn");
    }
    if (!rr::off::kCanWorldDistance) {
        out.emplace_back("world distance unavailable -- distance culls and readouts are "
                         "in screen pixels only");
    }
    if (!rr::off::kHasLocalPlayer) {
        out.emplace_back("local player unset -- own entity is not excluded from the list");
    }
    if (!rr::off::kHasEntityCount) {
        out.emplace_back("entity count unset -- the loop walks " +
                         std::to_string(rr::off::kEntityCountFallback) + " slots blind");
    }
    if (main_.size != 0 && rr::config::kHasExpectedImageSize &&
        main_.size != rr::config::kExpectedImageSize) {
        out.emplace_back("image size does not match the pinned build -- every offset here "
                         "is stale until re-dumped");
    }
    return out;
}

std::size_t World::read_count() const {
    if (rr::off::kHasEntityCount) {
        const auto live = proc_.read<std::int32_t>(main_.base + rr::off::kEntityCount);
        if (live.has_value() && *live > 0) {
            return std::min(static_cast<std::size_t>(*live), rr::off::kEntityCountMax);
        }
        return 0;
    }
    return std::min(rr::off::kEntityCountFallback, rr::off::kEntityCountMax);
}

bool World::read_entity(std::uint64_t address, Entity& out) const {
    if (!rr::config::plausible_pointer(address)) return false;
    out = Entity{};
    out.address = address;

    if (rr::off::kHasAlive) {
        const auto alive = proc_.read<std::int32_t>(address + rr::off::kEntAlive);
        if (!alive.has_value() || *alive == 0) return false;
    }

    if (!rr::off::kHasPosition) return false;
    float pos[3]{};
    if (!proc_.read_raw(address + rr::off::kEntPosition, pos, sizeof(pos))) return false;
    out.position = rr::math::Vec3{pos[0], pos[1], pos[2]};
    if (!rr::math::plausible_world(out.position)) return false;
    out.has_position = true;

    if (rr::off::kHasHeadPosition) {
        float head[3]{};
        if (proc_.read_raw(address + rr::off::kEntHeadPosition, head, sizeof(head))) {
            const rr::math::Vec3 h{head[0], head[1], head[2]};
            if (rr::math::plausible_world(h)) {
                out.head = h;
                out.has_head = true;
            }
        }
    }

    if (rr::off::kHasHealth) {
        const auto hp = proc_.read<std::int32_t>(address + rr::off::kEntHealth);
        if (!hp.has_value()) return false;
        if (*hp < kHealthMin || *hp > kHealthMax) return false;
        out.health = *hp;
        out.has_health = true;
    }

    if (rr::off::kHasTeam) {
        const auto team = proc_.read<std::int32_t>(address + rr::off::kEntTeam);
        if (team.has_value()) {
            out.team = *team;
            out.has_team = true;
        }
    }

    if (rr::off::kHasVisible) {
        const auto flag = proc_.read<std::int32_t>(address + rr::off::kEntVisible);
        if (flag.has_value()) {
            out.visible = (*flag != 0);
            out.has_visible = true;
        }
    }

    if (rr::off::kHasBones) {
        std::uint64_t array_base = address + rr::off::kEntBoneArray;
        bool base_ok = true;
        if (rr::off::kBoneArrayIsPointer) {
            const auto ptr = proc_.read<std::uint64_t>(address + rr::off::kEntBoneArray);
            if (ptr.has_value() && rr::config::plausible_pointer(*ptr)) {
                array_base = *ptr;
            } else {
                base_ok = false;
            }
        }
        if (base_ok) {
            const std::size_t count = std::min(rr::off::kBoneCount, rr::off::kBoneCountMax);
            out.bones.resize(count);
            for (std::size_t i = 0; i < count; ++i) {
                float raw[3]{};
                const std::uint64_t at =
                    array_base + i * rr::off::kBoneStride + rr::off::kBonePositionOffset;
                rr::math::Vec3 point{kBadCoord, kBadCoord, kBadCoord};
                if (proc_.read_raw(at, raw, sizeof(raw))) {
                    const rr::math::Vec3 candidate{raw[0], raw[1], raw[2]};
                    if (rr::math::plausible_world(candidate)) point = candidate;
                }
                // Bad entries stay in place as non-finite so bone indices keep meaning.
                out.bones[i] = point;
            }
        }
    }

    if (rr::off::kHasName) {
        char raw[rr::off::kNameLength + 1]{};
        if (proc_.read_raw(address + rr::off::kEntName, raw, rr::off::kNameLength)) {
            out.name = sanitize_name(raw, rr::off::kNameLength);
            out.has_name = !out.name.empty();
        }
    }

    return true;
}

bool World::snapshot(Snapshot& out) {
    out.entities.clear();
    out.has_local = false;
    out.has_view = false;
    out.rejected = 0;
    out.local = Entity{};
    out.frame = ++frame_;

    if (!attached_ || !proc_.valid()) return false;
    if (!proc_.alive()) {
        attached_ = false;
        return false;
    }

    if (rr::off::kHasViewMatrix) {
        float raw[16]{};
        if (proc_.read_raw(main_.base + rr::off::kViewMatrix, raw, sizeof(raw))) {
            const rr::math::Mat4 mat = rr::math::mat4_from_floats(raw);
            if (rr::math::mat4_finite(mat)) {
                out.view = mat;
                out.has_view = true;
            }
        }
    }

    std::uint64_t local_address = 0;
    if (rr::off::kHasLocalPlayer) {
        const auto ptr = proc_.read<std::uint64_t>(main_.base + rr::off::kLocalPlayer);
        if (ptr.has_value() && rr::config::plausible_pointer(*ptr)) {
            local_address = *ptr;
            Entity local;
            if (read_entity(local_address, local)) {
                out.local = local;
                out.has_local = true;
            }
        }
    }

    if (!rr::off::kCanEnumerate) return true;

    const std::uint64_t list_base = main_.base + rr::off::kEntityList;
    const std::size_t count = read_count();
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t entity_address = 0;
        if constexpr (rr::off::kEntityListIsPointerArray) {
            const auto ptr = proc_.read<std::uint64_t>(list_base + i * sizeof(std::uint64_t));
            if (!ptr.has_value()) {
                ++out.rejected;
                continue;
            }
            entity_address = *ptr;
        } else {
            entity_address = list_base + i * rr::off::kEntityStride;
        }

        if (!rr::config::plausible_pointer(entity_address)) {
            ++out.rejected;
            continue;
        }
        if (entity_address == local_address) continue;

        Entity ent;
        if (!read_entity(entity_address, ent)) {
            ++out.rejected;
            continue;
        }
        out.entities.push_back(std::move(ent));
    }

    if (out.has_local && out.local.has_position) {
        for (auto& ent : out.entities) {
            if (!ent.has_position) continue;
            const float distance = rr::math::length(ent.position - out.local.position);
            if (!rr::math::finite(distance)) continue;
            ent.distance_world = distance;
            ent.has_distance = true;
        }
    }
    return true;
}

} // namespace rr::game
