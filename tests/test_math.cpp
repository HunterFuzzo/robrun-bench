// Checks the projection and the aim math without the game: both matrix readings,
// the behind-camera case, non-finite rejection, smoothing and the raise axis.
#include <cmath>
#include <cstdio>

#include "entity.hpp"
#include "math.hpp"
#include "offsets.hpp"

namespace {

int g_failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::printf("FAIL %s\n", what);
        ++g_failures;
    }
}

bool near_equal(float a, float b, float tolerance) {
    return std::fabs(a - b) <= tolerance;
}

// Row-major view matrix with the clip w taken from x, screen x from y and screen
// y from z. A point at (1, 0, 0) therefore lands dead centre, and (1, 1, 0) lands
// on the right edge.
rr::math::Mat4 row_major_identity() {
    const float raw[16] = {0.0f, 1.0f, 0.0f, 0.0f,
                           0.0f, 0.0f, 1.0f, 0.0f,
                           0.0f, 0.0f, 1.0f, 0.0f,
                           1.0f, 0.0f, 0.0f, 0.0f};
    return rr::math::mat4_from_floats(raw);
}

void test_projection_centre() {
    const auto mat = row_major_identity();
    rr::math::Vec2 out;
    const bool ok = rr::math::world_to_screen(mat, true, rr::math::Vec3{1.0f, 0.0f, 0.0f},
                                              1920.0f, 1080.0f, out);
    check(ok, "point in front projects");
    check(near_equal(out.x, 960.0f, 0.5f), "x lands on screen centre");
    check(near_equal(out.y, 540.0f, 0.5f), "y lands on screen centre");
}

void test_projection_offset() {
    const auto mat = row_major_identity();
    rr::math::Vec2 out;
    const bool ok = rr::math::world_to_screen(mat, true, rr::math::Vec3{1.0f, 1.0f, 0.0f},
                                              1920.0f, 1080.0f, out);
    check(ok, "offset point projects");
    check(near_equal(out.x, 1920.0f, 0.5f), "ndc 1 maps to the right edge");
    check(near_equal(out.y, 540.0f, 0.5f), "y unchanged");
}

void test_behind_camera() {
    const auto mat = row_major_identity();
    rr::math::Vec2 out{-1.0f, -1.0f};
    check(!rr::math::world_to_screen(mat, true, rr::math::Vec3{-5.0f, 0.0f, 0.0f}, 1920.0f,
                                     1080.0f, out),
          "point behind the camera is rejected");
    check(!rr::math::world_to_screen(mat, true, rr::math::Vec3{0.0f, 0.0f, 0.0f}, 1920.0f,
                                     1080.0f, out),
          "point on the camera plane is rejected");
}

void test_convention_switch() {
    // Same arrangement read down the columns instead of across the rows.
    const float raw[16] = {0.0f, 0.0f, 0.0f, 1.0f,
                           1.0f, 0.0f, 0.0f, 0.0f,
                           0.0f, 1.0f, 0.0f, 0.0f,
                           0.0f, 0.0f, 0.0f, 0.0f};
    const auto mat = rr::math::mat4_from_floats(raw);
    rr::math::Vec2 out;
    check(rr::math::world_to_screen(mat, false, rr::math::Vec3{1.0f, 0.0f, 0.0f}, 1920.0f,
                                    1080.0f, out),
          "column-major reading projects");
    check(near_equal(out.x, 960.0f, 0.5f), "column-major x centre");
    check(!rr::math::world_to_screen(mat, true, rr::math::Vec3{1.0f, 0.0f, 0.0f}, 1920.0f,
                                     1080.0f, out),
          "wrong reading of the same matrix clips instead of lying");
}

void test_rejections() {
    auto mat = row_major_identity();
    rr::math::Vec2 out;
    mat.m[2][2] = std::nanf("");
    check(!rr::math::world_to_screen(mat, true, rr::math::Vec3{1.0f, 0.0f, 0.0f}, 1920.0f,
                                     1080.0f, out),
          "non-finite matrix rejected");

    const auto good = row_major_identity();
    check(!rr::math::world_to_screen(good, true,
                                     rr::math::Vec3{1.0f, std::nanf(""), 0.0f}, 1920.0f,
                                     1080.0f, out),
          "non-finite world point rejected");
    check(!rr::math::world_to_screen(good, true, rr::math::Vec3{2.0e6f, 0.0f, 0.0f},
                                     1920.0f, 1080.0f, out),
          "absurd coordinate rejected");
    check(!rr::math::world_to_screen(good, true, rr::math::Vec3{1.0f, 0.0f, 0.0f}, 0.0f,
                                     0.0f, out),
          "zero viewport rejected");
    check(!rr::math::plausible_world(rr::math::Vec3{0.0f, 0.0f, 1.0e9f}),
          "plausible_world rejects a far coordinate");
}

void test_smoothing() {
    const rr::math::Vec2 from{100.0f, 100.0f};
    const rr::math::Vec2 to{200.0f, 100.0f};
    const auto snap = rr::math::smooth_step(from, to, 1.0f);
    check(near_equal(snap.x, 100.0f, 0.01f), "smoothing 1 covers the whole delta");
    const auto soft = rr::math::smooth_step(from, to, 10.0f);
    check(near_equal(soft.x, 10.0f, 0.01f), "smoothing 10 covers a tenth");
    const auto guarded = rr::math::smooth_step(from, to, 0.0f);
    check(near_equal(guarded.x, 100.0f, 0.01f), "smoothing below 1 is clamped, not divided");
    check(near_equal(rr::math::screen_distance(rr::math::Vec2{3.0f, 4.0f},
                                               rr::math::Vec2{0.0f, 0.0f}),
                     5.0f, 0.01f),
          "screen distance is euclidean");
}

void test_raise_axis() {
    const rr::math::Vec3 base{1.0f, 2.0f, 3.0f};
    const auto up = rr::game::raise(base, 10.0f);
    if constexpr (rr::off::kUpAxisIndex == 2) {
        check(near_equal(up.z, 13.0f, 0.01f), "raise moves the z component");
        check(near_equal(up.x, 1.0f, 0.01f), "raise leaves x alone");
    } else if constexpr (rr::off::kUpAxisIndex == 1) {
        check(near_equal(up.y, 12.0f, 0.01f), "raise moves the y component");
    } else {
        check(near_equal(up.x, 11.0f, 0.01f), "raise moves the x component");
    }
}

void test_clamps() {
    check(near_equal(rr::math::clampf(5.0f, 0.0f, 1.0f), 1.0f, 0.01f), "clampf high");
    check(near_equal(rr::math::clampf(-5.0f, 0.0f, 1.0f), 0.0f, 0.01f), "clampf low");
    check(rr::math::clampi(7, 0, 3) == 3, "clampi high");
    check(rr::math::clampi(-7, 0, 3) == 0, "clampi low");
}

} // namespace

int main() {
    test_projection_centre();
    test_projection_offset();
    test_behind_camera();
    test_convention_switch();
    test_rejections();
    test_smoothing();
    test_raise_axis();
    test_clamps();
    if (g_failures == 0) {
        std::printf("all math checks passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
}
