#pragma once
// Layered click-through window with a GDI back buffer. Colour-key transparency:
// pure black is the key, so no drawn element may use RGB(0, 0, 0).
//
// Every GDI object is owned here and released in the destructor and on resize.
// The window follows the target's client rect each frame, so resize, move and a
// DPI change are all handled by the same path.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <string>

#include "math.hpp"

namespace rr::ui {

inline constexpr COLORREF kKeyColor = RGB(0, 0, 0);
inline constexpr COLORREF kWhite = RGB(235, 235, 235);
inline constexpr COLORREF kGrey = RGB(140, 140, 146);
inline constexpr COLORREF kDark = RGB(18, 18, 20);
inline constexpr COLORREF kPanel = RGB(28, 28, 32);
inline constexpr COLORREF kAccent = RGB(120, 190, 255);
inline constexpr COLORREF kGood = RGB(110, 210, 120);
inline constexpr COLORREF kBad = RGB(230, 90, 90);
inline constexpr COLORREF kWarn = RGB(240, 190, 90);
inline constexpr COLORREF kEnemy = RGB(230, 90, 90);
inline constexpr COLORREF kTeam = RGB(110, 210, 120);
inline constexpr COLORREF kOutline = RGB(10, 10, 12);

// Returns the first visible top-level window owned by that pid, largest first.
HWND find_window_for_pid(DWORD pid);

class Overlay {
public:
    Overlay() = default;
    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;
    ~Overlay();

    bool create(const wchar_t* class_name);
    void destroy();
    [[nodiscard]] bool valid() const { return hwnd_ != nullptr; }
    [[nodiscard]] HWND hwnd() const { return hwnd_; }
    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }
    // Screen coordinates of the overlay's top-left corner, updated by follow().
    [[nodiscard]] rr::math::Vec2 origin() const { return origin_; }
    [[nodiscard]] rr::math::Vec2 centre() const {
        return rr::math::Vec2{static_cast<float>(width_) * 0.5f,
                              static_cast<float>(height_) * 0.5f};
    }

    // Matches position and size to the target's client area. False means the
    // target window went away.
    bool follow(HWND target);
    void set_click_through(bool enabled);
    void pump_messages();

    void begin_frame();
    void end_frame();

    void line(float x1, float y1, float x2, float y2, COLORREF colour, int thickness = 1);
    void rect(float x, float y, float w, float h, COLORREF colour, int thickness = 1);
    void filled_rect(float x, float y, float w, float h, COLORREF colour);
    void outlined_rect(float x, float y, float w, float h, COLORREF colour);
    void circle(float cx, float cy, float radius, COLORREF colour, int thickness = 1);
    void filled_circle(float cx, float cy, float radius, COLORREF colour);
    void text(float x, float y, const std::string& value, COLORREF colour,
              bool small_font = true);
    void text_centered(float x, float y, const std::string& value, COLORREF colour,
                       bool small_font = true);
    [[nodiscard]] int text_width(const std::string& value, bool small_font = true) const;
    [[nodiscard]] int line_height(bool small_font = true) const;

private:
    bool resize_buffers(int width, int height);
    void release_buffers();

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    std::wstring class_name_;
    HDC window_dc_ = nullptr;
    HDC memory_dc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HBITMAP old_bitmap_ = nullptr;
    HFONT font_small_ = nullptr;
    HFONT font_large_ = nullptr;
    rr::math::Vec2 origin_;
    int width_ = 0;
    int height_ = 0;
    bool click_through_ = true;
};

} // namespace rr::ui
