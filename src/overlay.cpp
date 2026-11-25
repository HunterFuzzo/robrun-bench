#include "overlay.hpp"

namespace rr::ui {
namespace {

struct FindState {
    DWORD pid = 0;
    HWND best = nullptr;
    long best_area = 0;
};

BOOL CALLBACK enum_proc(HWND hwnd, LPARAM param) {
    auto* state = reinterpret_cast<FindState*>(param);
    DWORD owner = 0;
    GetWindowThreadProcessId(hwnd, &owner);
    if (owner != state->pid) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;

    RECT rc{};
    if (!GetClientRect(hwnd, &rc)) return TRUE;
    const long area = (rc.right - rc.left) * (rc.bottom - rc.top);
    if (area <= 0) return TRUE;
    if (area > state->best_area) {
        state->best_area = area;
        state->best = hwnd;
    }
    return TRUE;
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void make_dpi_aware() {
    using SetContext = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32 != nullptr) {
        const auto fn = reinterpret_cast<SetContext>(
            reinterpret_cast<void*>(GetProcAddress(user32, "SetProcessDpiAwarenessContext")));
        if (fn != nullptr &&
            fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE) {
            return;
        }
    }
    SetProcessDPIAware();
}

} // namespace

HWND find_window_for_pid(DWORD pid) {
    FindState state;
    state.pid = pid;
    EnumWindows(enum_proc, reinterpret_cast<LPARAM>(&state));
    return state.best;
}

Overlay::~Overlay() { destroy(); }

bool Overlay::create(const wchar_t* class_name) {
    make_dpi_aware();
    instance_ = GetModuleHandleW(nullptr);
    class_name_ = class_name;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance_;
    wc.lpszClassName = class_name_.c_str();
    wc.hbrBackground = nullptr;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
                                WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                            class_name_.c_str(), L"", WS_POPUP, 0, 0, 16, 16, nullptr,
                            nullptr, instance_, nullptr);
    if (hwnd_ == nullptr) {
        destroy();
        return false;
    }

    if (!SetLayeredWindowAttributes(hwnd_, kKeyColor, 0, LWA_COLORKEY)) {
        destroy();
        return false;
    }

    window_dc_ = GetDC(hwnd_);
    if (window_dc_ == nullptr) {
        destroy();
        return false;
    }

    font_small_ = CreateFontW(14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, FF_DONTCARE, L"Consolas");
    font_large_ = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              FF_DONTCARE, L"Consolas");
    if (font_small_ == nullptr || font_large_ == nullptr) {
        destroy();
        return false;
    }

    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    return true;
}

void Overlay::release_buffers() {
    if (memory_dc_ != nullptr) {
        if (old_bitmap_ != nullptr) {
            SelectObject(memory_dc_, old_bitmap_);
            old_bitmap_ = nullptr;
        }
        DeleteDC(memory_dc_);
        memory_dc_ = nullptr;
    }
    if (bitmap_ != nullptr) {
        DeleteObject(bitmap_);
        bitmap_ = nullptr;
    }
}

void Overlay::destroy() {
    release_buffers();
    if (font_small_ != nullptr) {
        DeleteObject(font_small_);
        font_small_ = nullptr;
    }
    if (font_large_ != nullptr) {
        DeleteObject(font_large_);
        font_large_ = nullptr;
    }
    if (window_dc_ != nullptr && hwnd_ != nullptr) {
        ReleaseDC(hwnd_, window_dc_);
    }
    window_dc_ = nullptr;
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (!class_name_.empty() && instance_ != nullptr) {
        UnregisterClassW(class_name_.c_str(), instance_);
        class_name_.clear();
    }
    width_ = 0;
    height_ = 0;
}

bool Overlay::resize_buffers(int width, int height) {
    if (width <= 0 || height <= 0) return false;
    if (width == width_ && height == height_ && memory_dc_ != nullptr) return true;

    release_buffers();
    memory_dc_ = CreateCompatibleDC(window_dc_);
    if (memory_dc_ == nullptr) return false;
    bitmap_ = CreateCompatibleBitmap(window_dc_, width, height);
    if (bitmap_ == nullptr) {
        release_buffers();
        return false;
    }
    old_bitmap_ = static_cast<HBITMAP>(SelectObject(memory_dc_, bitmap_));
    SetBkMode(memory_dc_, TRANSPARENT);
    width_ = width;
    height_ = height;
    return true;
}

bool Overlay::follow(HWND target) {
    if (hwnd_ == nullptr || target == nullptr || !IsWindow(target)) return false;

    RECT client{};
    if (!GetClientRect(target, &client)) return false;
    POINT top_left{client.left, client.top};
    if (!ClientToScreen(target, &top_left)) return false;

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return false;

    RECT own{};
    GetWindowRect(hwnd_, &own);
    if (own.left != top_left.x || own.top != top_left.y ||
        (own.right - own.left) != width || (own.bottom - own.top) != height) {
        SetWindowPos(hwnd_, HWND_TOPMOST, top_left.x, top_left.y, width, height,
                     SWP_NOACTIVATE | SWP_NOSENDCHANGING);
    }
    origin_ = rr::math::Vec2{static_cast<float>(top_left.x), static_cast<float>(top_left.y)};
    return resize_buffers(width, height);
}

void Overlay::set_click_through(bool enabled) {
    if (hwnd_ == nullptr || enabled == click_through_) return;
    LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    if (enabled) {
        style |= WS_EX_TRANSPARENT;
    } else {
        style &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    }
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, style);
    click_through_ = enabled;
}

void Overlay::pump_messages() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void Overlay::begin_frame() {
    if (memory_dc_ == nullptr) return;
    const RECT full{0, 0, width_, height_};
    HBRUSH brush = CreateSolidBrush(kKeyColor);
    if (brush != nullptr) {
        FillRect(memory_dc_, &full, brush);
        DeleteObject(brush);
    }
}

void Overlay::end_frame() {
    if (memory_dc_ == nullptr || window_dc_ == nullptr) return;
    BitBlt(window_dc_, 0, 0, width_, height_, memory_dc_, 0, 0, SRCCOPY);
}

void Overlay::line(float x1, float y1, float x2, float y2, COLORREF colour, int thickness) {
    if (memory_dc_ == nullptr) return;
    HPEN pen = CreatePen(PS_SOLID, thickness, colour);
    if (pen == nullptr) return;
    HGDIOBJ old_pen = SelectObject(memory_dc_, pen);
    MoveToEx(memory_dc_, static_cast<int>(x1), static_cast<int>(y1), nullptr);
    LineTo(memory_dc_, static_cast<int>(x2), static_cast<int>(y2));
    SelectObject(memory_dc_, old_pen);
    DeleteObject(pen);
}

void Overlay::rect(float x, float y, float w, float h, COLORREF colour, int thickness) {
    if (memory_dc_ == nullptr) return;
    HPEN pen = CreatePen(PS_SOLID, thickness, colour);
    if (pen == nullptr) return;
    HGDIOBJ old_pen = SelectObject(memory_dc_, pen);
    HGDIOBJ old_brush = SelectObject(memory_dc_, GetStockObject(NULL_BRUSH));
    Rectangle(memory_dc_, static_cast<int>(x), static_cast<int>(y), static_cast<int>(x + w),
              static_cast<int>(y + h));
    SelectObject(memory_dc_, old_brush);
    SelectObject(memory_dc_, old_pen);
    DeleteObject(pen);
}

void Overlay::filled_rect(float x, float y, float w, float h, COLORREF colour) {
    if (memory_dc_ == nullptr) return;
    const RECT area{static_cast<int>(x), static_cast<int>(y), static_cast<int>(x + w),
                    static_cast<int>(y + h)};
    HBRUSH brush = CreateSolidBrush(colour);
    if (brush == nullptr) return;
    FillRect(memory_dc_, &area, brush);
    DeleteObject(brush);
}

void Overlay::outlined_rect(float x, float y, float w, float h, COLORREF colour) {
    rect(x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, kOutline, 1);
    rect(x, y, w, h, colour, 1);
    rect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, kOutline, 1);
}

void Overlay::circle(float cx, float cy, float radius, COLORREF colour, int thickness) {
    if (memory_dc_ == nullptr || radius < 1.0f) return;
    HPEN pen = CreatePen(PS_SOLID, thickness, colour);
    if (pen == nullptr) return;
    HGDIOBJ old_pen = SelectObject(memory_dc_, pen);
    HGDIOBJ old_brush = SelectObject(memory_dc_, GetStockObject(NULL_BRUSH));
    Ellipse(memory_dc_, static_cast<int>(cx - radius), static_cast<int>(cy - radius),
            static_cast<int>(cx + radius), static_cast<int>(cy + radius));
    SelectObject(memory_dc_, old_brush);
    SelectObject(memory_dc_, old_pen);
    DeleteObject(pen);
}

void Overlay::filled_circle(float cx, float cy, float radius, COLORREF colour) {
    if (memory_dc_ == nullptr || radius < 1.0f) return;
    HPEN pen = CreatePen(PS_SOLID, 1, colour);
    HBRUSH brush = CreateSolidBrush(colour);
    if (pen == nullptr || brush == nullptr) {
        if (pen != nullptr) DeleteObject(pen);
        if (brush != nullptr) DeleteObject(brush);
        return;
    }
    HGDIOBJ old_pen = SelectObject(memory_dc_, pen);
    HGDIOBJ old_brush = SelectObject(memory_dc_, brush);
    Ellipse(memory_dc_, static_cast<int>(cx - radius), static_cast<int>(cy - radius),
            static_cast<int>(cx + radius), static_cast<int>(cy + radius));
    SelectObject(memory_dc_, old_brush);
    SelectObject(memory_dc_, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void Overlay::text(float x, float y, const std::string& value, COLORREF colour,
                   bool small_font) {
    if (memory_dc_ == nullptr || value.empty()) return;
    HGDIOBJ old_font = SelectObject(memory_dc_, small_font ? font_small_ : font_large_);
    SetBkMode(memory_dc_, TRANSPARENT);
    SetTextColor(memory_dc_, kOutline);
    TextOutA(memory_dc_, static_cast<int>(x) + 1, static_cast<int>(y) + 1, value.c_str(),
             static_cast<int>(value.size()));
    SetTextColor(memory_dc_, colour);
    TextOutA(memory_dc_, static_cast<int>(x), static_cast<int>(y), value.c_str(),
             static_cast<int>(value.size()));
    SelectObject(memory_dc_, old_font);
}

void Overlay::text_centered(float x, float y, const std::string& value, COLORREF colour,
                            bool small_font) {
    const int width = text_width(value, small_font);
    text(x - static_cast<float>(width) * 0.5f, y, value, colour, small_font);
}

int Overlay::text_width(const std::string& value, bool small_font) const {
    if (memory_dc_ == nullptr || value.empty()) return 0;
    HGDIOBJ old_font = SelectObject(memory_dc_, small_font ? font_small_ : font_large_);
    SIZE size{};
    GetTextExtentPoint32A(memory_dc_, value.c_str(), static_cast<int>(value.size()), &size);
    SelectObject(memory_dc_, old_font);
    return size.cx;
}

int Overlay::line_height(bool small_font) const {
    if (memory_dc_ == nullptr) return small_font ? 14 : 18;
    HGDIOBJ old_font = SelectObject(memory_dc_, small_font ? font_small_ : font_large_);
    TEXTMETRICA tm{};
    GetTextMetricsA(memory_dc_, &tm);
    SelectObject(memory_dc_, old_font);
    return static_cast<int>(tm.tmHeight);
}

} // namespace rr::ui
