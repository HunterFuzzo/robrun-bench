#include "input.hpp"

#include <cstdio>

namespace rr::input {
namespace {

bool g_left_held = false;

} // namespace

void Input::poll() {
    prev_ = now_;
    for (std::size_t vk = 0; vk < kKeyCount; ++vk) {
        now_[vk] = (GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) != 0;
    }
    cursor_prev_ = cursor_;
    POINT p{};
    if (GetCursorPos(&p)) {
        cursor_.x = static_cast<float>(p.x);
        cursor_.y = static_cast<float>(p.y);
    }
}

bool Input::down(int vk) const {
    if (vk <= 0 || static_cast<std::size_t>(vk) >= kKeyCount) return false;
    return now_[static_cast<std::size_t>(vk)];
}

bool Input::pressed(int vk) const {
    if (vk <= 0 || static_cast<std::size_t>(vk) >= kKeyCount) return false;
    const std::size_t i = static_cast<std::size_t>(vk);
    return now_[i] && !prev_[i];
}

bool Input::released(int vk) const {
    if (vk <= 0 || static_cast<std::size_t>(vk) >= kKeyCount) return false;
    const std::size_t i = static_cast<std::size_t>(vk);
    return !now_[i] && prev_[i];
}

rr::math::Vec2 Input::cursor_delta() const {
    return rr::math::Vec2{cursor_.x - cursor_prev_.x, cursor_.y - cursor_prev_.y};
}

int Input::first_pressed() const {
    for (std::size_t vk = 1; vk < kKeyCount; ++vk) {
        if (now_[vk] && !prev_[vk]) return static_cast<int>(vk);
    }
    return 0;
}

void move_mouse(int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_MOVE;
    in.mi.dx = dx;
    in.mi.dy = dy;
    SendInput(1, &in, sizeof(in));
}

void press_left() {
    if (g_left_held) return;
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &in, sizeof(in));
    g_left_held = true;
}

void release_left() {
    if (!g_left_held) return;
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &in, sizeof(in));
    g_left_held = false;
}

const char* key_name(int vk) {
    static char buffer[32]{};
    switch (vk) {
        case 0:            return "none";
        case VK_LBUTTON:   return "mouse1";
        case VK_RBUTTON:   return "mouse2";
        case VK_MBUTTON:   return "mouse3";
        case VK_XBUTTON1:  return "mouse4";
        case VK_XBUTTON2:  return "mouse5";
        case VK_SHIFT:     return "shift";
        case VK_CONTROL:   return "ctrl";
        case VK_MENU:      return "alt";
        case VK_SPACE:     return "space";
        case VK_TAB:       return "tab";
        case VK_CAPITAL:   return "caps";
        case VK_INSERT:    return "insert";
        case VK_DELETE:    return "delete";
        case VK_HOME:      return "home";
        case VK_END:       return "end";
        case VK_PRIOR:     return "pgup";
        case VK_NEXT:      return "pgdn";
        case VK_ESCAPE:    return "escape";
        default: break;
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        std::snprintf(buffer, sizeof(buffer), "f%d", vk - VK_F1 + 1);
        return buffer;
    }
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) {
        std::snprintf(buffer, sizeof(buffer), "%c", static_cast<char>(vk));
        return buffer;
    }
    std::snprintf(buffer, sizeof(buffer), "vk_%02X", vk);
    return buffer;
}

} // namespace rr::input
