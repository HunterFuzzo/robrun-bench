#pragma once
// Polled input. GetAsyncKeyState rather than a message loop, so the overlay never
// needs focus to read a hotkey.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <cstddef>

#include "math.hpp"

namespace rr::input {

class Input {
public:
    void poll();

    [[nodiscard]] bool down(int vk) const;
    [[nodiscard]] bool pressed(int vk) const;   // edge: up last frame, down now
    [[nodiscard]] bool released(int vk) const;  // edge: down last frame, up now
    [[nodiscard]] rr::math::Vec2 cursor() const { return cursor_; }
    [[nodiscard]] rr::math::Vec2 cursor_delta() const;

    // The first key that went down this frame, or 0. Used by the keybind widget.
    [[nodiscard]] int first_pressed() const;

private:
    static constexpr std::size_t kKeyCount = 256;
    std::array<bool, kKeyCount> now_{};
    std::array<bool, kKeyCount> prev_{};
    rr::math::Vec2 cursor_;
    rr::math::Vec2 cursor_prev_;
};

// Relative mouse move. SendInput only: nothing is written into the target process.
void move_mouse(int dx, int dy);
void press_left();
void release_left();

// Human-readable name for a virtual key, for the keybind widget.
const char* key_name(int vk);

} // namespace rr::input
