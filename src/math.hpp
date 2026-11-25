#pragma once
// Pure math. No platform calls and no game reads, so every line here is checkable
// without the process running.
//
// Coordinate convention: world is a right-handed float triple, Z or Y up is decided
// by the game and does not matter to the projection below. The view matrix is 16
// floats; whether the rows or the columns carry the basis is unconfirmed, so both
// readings are implemented and rr::off::kMatrixRowMajor picks one. Screen space is
// pixels, origin top-left, Y down. Angles are degrees where they appear.
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace rr::math {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Mat4 {
    float m[4][4]{};
};

inline Vec2 operator-(const Vec2& a, const Vec2& b) { return Vec2{a.x - b.x, a.y - b.y}; }
inline Vec2 operator+(const Vec2& a, const Vec2& b) { return Vec2{a.x + b.x, a.y + b.y}; }
inline Vec2 operator*(const Vec2& a, float s) { return Vec2{a.x * s, a.y * s}; }
inline Vec3 operator-(const Vec3& a, const Vec3& b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}
inline Vec3 operator+(const Vec3& a, const Vec3& b) {
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

inline float length(const Vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }
inline float length(const Vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

inline bool finite(float v) { return std::isfinite(v); }
inline bool finite(const Vec3& v) { return finite(v.x) && finite(v.y) && finite(v.z); }
inline bool finite(const Vec2& v) { return finite(v.x) && finite(v.y); }

// A coordinate further out than this is a failed read, not a far entity.
inline constexpr float kWorldLimit = 1.0e6f;

inline bool plausible_world(const Vec3& v) {
    if (!finite(v)) return false;
    return std::fabs(v.x) < kWorldLimit && std::fabs(v.y) < kWorldLimit &&
           std::fabs(v.z) < kWorldLimit;
}

inline Mat4 mat4_from_floats(const float (&raw)[16]) {
    Mat4 out;
    for (std::size_t r = 0; r < 4; ++r) {
        for (std::size_t c = 0; c < 4; ++c) {
            out.m[r][c] = raw[r * 4 + c];
        }
    }
    return out;
}

inline bool mat4_finite(const Mat4& mat) {
    for (std::size_t r = 0; r < 4; ++r) {
        for (std::size_t c = 0; c < 4; ++c) {
            if (!finite(mat.m[r][c])) return false;
        }
    }
    return true;
}

// Below this clip w the point is on or behind the camera plane and has no screen
// position at all. Returning false is the only correct answer there.
inline constexpr float kClipW = 0.01f;

inline bool world_to_screen_row_major(const Mat4& mat, const Vec3& world, float screen_w,
                                      float screen_h, Vec2& out) {
    const float w = mat.m[3][0] * world.x + mat.m[3][1] * world.y + mat.m[3][2] * world.z +
                    mat.m[3][3];
    if (!finite(w) || w < kClipW) return false;
    const float px = mat.m[0][0] * world.x + mat.m[0][1] * world.y + mat.m[0][2] * world.z +
                     mat.m[0][3];
    const float py = mat.m[1][0] * world.x + mat.m[1][1] * world.y + mat.m[1][2] * world.z +
                     mat.m[1][3];
    const float ndc_x = px / w;
    const float ndc_y = py / w;
    if (!finite(ndc_x) || !finite(ndc_y)) return false;
    out.x = (screen_w * 0.5f) * (1.0f + ndc_x);
    out.y = (screen_h * 0.5f) * (1.0f - ndc_y);
    return finite(out);
}

inline bool world_to_screen_col_major(const Mat4& mat, const Vec3& world, float screen_w,
                                      float screen_h, Vec2& out) {
    const float w = mat.m[0][3] * world.x + mat.m[1][3] * world.y + mat.m[2][3] * world.z +
                    mat.m[3][3];
    if (!finite(w) || w < kClipW) return false;
    const float px = mat.m[0][0] * world.x + mat.m[1][0] * world.y + mat.m[2][0] * world.z +
                     mat.m[3][0];
    const float py = mat.m[0][1] * world.x + mat.m[1][1] * world.y + mat.m[2][1] * world.z +
                     mat.m[3][1];
    const float ndc_x = px / w;
    const float ndc_y = py / w;
    if (!finite(ndc_x) || !finite(ndc_y)) return false;
    out.x = (screen_w * 0.5f) * (1.0f + ndc_x);
    out.y = (screen_h * 0.5f) * (1.0f - ndc_y);
    return finite(out);
}

inline bool world_to_screen(const Mat4& mat, bool row_major, const Vec3& world,
                            float screen_w, float screen_h, Vec2& out) {
    if (!plausible_world(world) || !mat4_finite(mat)) return false;
    if (screen_w <= 1.0f || screen_h <= 1.0f) return false;
    return row_major ? world_to_screen_row_major(mat, world, screen_w, screen_h, out)
                     : world_to_screen_col_major(mat, world, screen_w, screen_h, out);
}

// FOV is measured in screen pixels from the crosshair, not in degrees: no camera
// intrinsics are confirmed on this build, and pixels need none.
inline float screen_distance(const Vec2& point, const Vec2& crosshair) {
    return length(point - crosshair);
}

// One exponential step toward the target. smoothing 1 means snap; larger values
// divide the step. Returns the delta to apply this frame.
inline Vec2 smooth_step(const Vec2& from, const Vec2& to, float smoothing) {
    const float s = smoothing < 1.0f ? 1.0f : smoothing;
    const Vec2 delta = to - from;
    return Vec2{delta.x / s, delta.y / s};
}

inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

inline int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

} // namespace rr::math
