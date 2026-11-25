// Checks hitbox resolution, target filtering and nearest-point selection. Pure:
// no window, no process, no game.
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "entity.hpp"
#include "math.hpp"
#include "offsets.hpp"
#include "target.hpp"

namespace {

int g_failures = 0;
constexpr float kHeight = 72.0f;

void check(bool condition, const char* what) {
    if (!condition) {
        std::printf("FAIL %s\n", what);
        ++g_failures;
    }
}

bool same_point(const rr::math::Vec3& a, const rr::math::Vec3& b) {
    return std::fabs(a.x - b.x) < 0.01f && std::fabs(a.y - b.y) < 0.01f &&
           std::fabs(a.z - b.z) < 0.01f;
}

rr::game::Entity make_entity(float x, float y, float z) {
    rr::game::Entity ent;
    ent.address = 0x1000;
    ent.position = rr::math::Vec3{x, y, z};
    ent.has_position = true;
    return ent;
}

void test_hitbox_enum() {
    check(rr::aimlogic::hitbox_from_int(-1) == rr::aimlogic::Hitbox::BodyPoint,
          "negative index falls back to body");
    check(rr::aimlogic::hitbox_from_int(99) == rr::aimlogic::Hitbox::BodyPoint,
          "out of range index falls back to body");
    check(rr::aimlogic::hitbox_from_int(1) == rr::aimlogic::Hitbox::UpperPoint,
          "index 1 is the upper point");
    check(std::string(rr::aimlogic::hitbox_name(rr::aimlogic::Hitbox::NearestBone)) ==
              "nearest bone",
          "nearest bone names itself");
    check(static_cast<int>(rr::aimlogic::Hitbox::Count) == 5, "five hitbox modes");
}

void test_availability_matches_offsets() {
    check(rr::aimlogic::hitbox_available(rr::aimlogic::Hitbox::BodyPoint),
          "body point always available");
    check(rr::aimlogic::hitbox_available(rr::aimlogic::Hitbox::UpperPoint),
          "upper point always available");
    check(rr::aimlogic::hitbox_available(rr::aimlogic::Hitbox::HeadBone) ==
              rr::off::kHasHeadBone,
          "head bone availability tracks its offset gate");
    check(rr::aimlogic::hitbox_available(rr::aimlogic::Hitbox::NearestBone) ==
              rr::off::kHasBones,
          "nearest bone availability tracks the bone gate");

    const auto resolved_head =
        rr::aimlogic::resolve_hitbox(rr::aimlogic::Hitbox::HeadBone);
    if (rr::off::kHasHeadBone) {
        check(resolved_head == rr::aimlogic::Hitbox::HeadBone, "head bone kept when gated on");
    } else {
        check(resolved_head == rr::aimlogic::Hitbox::UpperPoint,
              "head bone falls back to the upper point");
    }
    check(rr::aimlogic::resolve_hitbox(rr::aimlogic::Hitbox::BodyPoint) ==
              rr::aimlogic::Hitbox::BodyPoint,
          "body point never falls back");
}

void test_world_points() {
    auto ent = make_entity(10.0f, 20.0f, 30.0f);
    const rr::math::Vec3 feet =
        rr::off::kPositionIsFeet ? ent.position
                                 : rr::game::raise(ent.position, -kHeight * 0.5f);

    rr::math::Vec3 body;
    check(rr::aimlogic::hitbox_world_point(ent, rr::aimlogic::Hitbox::BodyPoint, kHeight,
                                           body),
          "body point resolves");
    check(same_point(body, rr::game::raise(feet, kHeight * 0.5f)),
          "body point sits at half height");

    rr::math::Vec3 upper;
    check(rr::aimlogic::hitbox_world_point(ent, rr::aimlogic::Hitbox::UpperPoint, kHeight,
                                           upper),
          "upper point resolves without a head offset");
    check(same_point(upper, rr::game::raise(feet, kHeight * 0.9f)),
          "upper point derives from the height");

    ent.head = rr::math::Vec3{1.0f, 2.0f, 3.0f};
    ent.has_head = true;
    check(rr::aimlogic::hitbox_world_point(ent, rr::aimlogic::Hitbox::UpperPoint, kHeight,
                                           upper),
          "upper point resolves with a head offset");
    check(same_point(upper, rr::math::Vec3{1.0f, 2.0f, 3.0f}),
          "a confirmed head read wins over the derived point");

    rr::math::Vec3 ignored;
    check(!rr::aimlogic::hitbox_world_point(ent, rr::aimlogic::Hitbox::NearestBone, kHeight,
                                            ignored),
          "nearest bone is not a world-space answer");

    rr::game::Entity no_position;
    check(!rr::aimlogic::hitbox_world_point(no_position, rr::aimlogic::Hitbox::BodyPoint,
                                            kHeight, ignored),
          "an entity without a position has no hitbox");
}

void test_bone_point() {
    auto ent = make_entity(0.0f, 0.0f, 0.0f);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    ent.bones = {rr::math::Vec3{1.0f, 1.0f, 1.0f}, rr::math::Vec3{nan, nan, nan},
                 rr::math::Vec3{2.0f, 2.0f, 2.0f}};

    rr::math::Vec3 out;
    check(rr::aimlogic::bone_point(ent, 0, out), "bone 0 reads");
    check(same_point(out, rr::math::Vec3{1.0f, 1.0f, 1.0f}), "bone 0 value");
    check(!rr::aimlogic::bone_point(ent, 1, out), "a non-finite bone slot is refused");
    check(rr::aimlogic::bone_point(ent, 2, out), "bone 2 still reads after the bad slot");
    check(same_point(out, rr::math::Vec3{2.0f, 2.0f, 2.0f}),
          "indices keep their meaning around a bad slot");
    check(!rr::aimlogic::bone_point(ent, 3, out), "out of range index refused");
    check(!rr::aimlogic::bone_point(ent, -1, out), "unset index refused");
}

void test_eligibility() {
    rr::game::Entity local = make_entity(0.0f, 0.0f, 0.0f);
    local.team = 1;
    local.has_team = true;

    auto enemy = make_entity(100.0f, 0.0f, 0.0f);
    enemy.team = 2;
    enemy.has_team = true;
    enemy.health = 100;
    enemy.has_health = true;

    rr::aimlogic::Filter filter;
    check(rr::aimlogic::eligible(enemy, local, true, filter), "live enemy is eligible");

    auto mate = enemy;
    mate.team = 1;
    check(!rr::aimlogic::eligible(mate, local, true, filter),
          "same team rejected while the check is on");
    filter.team_check = false;
    check(rr::aimlogic::eligible(mate, local, true, filter),
          "same team passes with the check off");
    filter.team_check = true;

    auto dead = enemy;
    dead.health = 0;
    check(!rr::aimlogic::eligible(dead, local, true, filter), "zero health rejected");

    rr::game::Entity no_position;
    check(!rr::aimlogic::eligible(no_position, local, true, filter),
          "no position means not eligible");

    filter.visible_only = true;
    check(rr::aimlogic::eligible(enemy, local, true, filter),
          "visible-only passes when the build has no visible flag");
    auto hidden = enemy;
    hidden.has_visible = true;
    hidden.visible = false;
    check(!rr::aimlogic::eligible(hidden, local, true, filter),
          "a read visible flag of zero is rejected");
    hidden.visible = true;
    check(rr::aimlogic::eligible(hidden, local, true, filter), "visible flag set passes");
    filter.visible_only = false;

    auto far_away = enemy;
    far_away.distance_world = 900.0f;
    far_away.has_distance = true;
    filter.max_world_distance = 500.0f;
    check(!rr::aimlogic::eligible(far_away, local, true, filter),
          "beyond the world cull is rejected");
    filter.max_world_distance = 1000.0f;
    check(rr::aimlogic::eligible(far_away, local, true, filter),
          "inside the world cull passes");
    filter.max_world_distance = 0.0f;
    check(rr::aimlogic::eligible(far_away, local, true, filter), "cull of zero is off");

    auto unknown_distance = enemy;
    filter.max_world_distance = 10.0f;
    check(rr::aimlogic::eligible(unknown_distance, local, true, filter),
          "an unread distance is not culled");
}

void test_nearest_point() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const std::vector<rr::math::Vec2> points = {
        rr::math::Vec2{100.0f, 100.0f}, rr::math::Vec2{nan, nan},
        rr::math::Vec2{10.0f, 10.0f}, rr::math::Vec2{500.0f, 500.0f}};
    const auto index = rr::aimlogic::nearest_point_index(points, rr::math::Vec2{0.0f, 0.0f});
    check(index.has_value() && *index == 2, "nearest point picked, bad slot skipped");

    const std::vector<rr::math::Vec2> only_bad = {rr::math::Vec2{nan, nan}};
    check(!rr::aimlogic::nearest_point_index(only_bad, rr::math::Vec2{0.0f, 0.0f})
               .has_value(),
          "all-bad input reports nothing");
    check(!rr::aimlogic::nearest_point_index({}, rr::math::Vec2{0.0f, 0.0f}).has_value(),
          "empty input reports nothing");
}

} // namespace

int main() {
    test_hitbox_enum();
    test_availability_matches_offsets();
    test_world_points();
    test_bone_point();
    test_eligibility();
    test_nearest_point();
    if (g_failures == 0) {
        std::printf("all target checks passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
}
